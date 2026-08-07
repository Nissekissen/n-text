#include <stdio.h>
#include <stdlib.h>
#include "renderer.h"
#include <unistd.h>
#include "rope.h"
#include "cursor.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>

void print_cursor_debug(Cursor* cursor, RopeNode* rope) {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size
    size_t total_newlines = rope_total_newlines(rope);
    char seq[64];
    int len = snprintf(seq, sizeof(seq),
        "\x1b""7"                 // DECSC: save cursor pos + attrs
        "\x1b[%d;%dH"              // move to bottom-right-ish corner
        "(%d,%d) %d, %d"                  // the debug text
        "\x1b""8",                 // DECRC: restore cursor pos + attrs
        ws.ws_row, ws.ws_col - 20, (int)cursor->row, (int)cursor->column, (int)total_newlines, (int)cursor->offset);

    write(STDOUT_FILENO, seq, len);
}

void save_file(RopeNode *root, char *dst) {
    char *buf = malloc(1024);
    size_t buf_len = 0, buf_cap = 1024;
    rope_collect(root, &buf, &buf_len, &buf_cap);

    if (buf_len == 0 || buf[buf_len - 1] != '\n') {
        safe_append(&buf, &buf_len, &buf_cap, "\n", 1);
    }

    int fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return; // TODO! add error handling here
    write(fd, buf, buf_len);
    close(fd);

    free(buf);
}

void load_file(RopeNode *root, char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return; // TODO! Handle error
    
    struct stat st;
    fstat(fd, &st);
    size_t size = st.st_size;

    char *buf = malloc(size);
    size_t total_read = 0;
    while (total_read < size) {
        ssize_t n = read(fd, buf + total_read, size - total_read);
        if (n <= 0) break;
        total_read += n;
    }

    close(fd);

    // split buf into LEAF_MAX_SIZE - 1 chunks
    size_t chunk_start = 0;
    while (chunk_start < size) {
        size_t chunk_size = (size - chunk_start) >= LEAF_MAX_SIZE ? LEAF_MAX_SIZE - 1 : (size - chunk_start);
        rope_insert(root, chunk_start, buf + chunk_start, chunk_size);
        chunk_start += chunk_size;
    }

    free(buf);
}

void scroll_to_cursor(Cursor *cursor, size_t *line_offset, struct winsize *ws) {
    if (cursor->row < *line_offset) *line_offset = cursor->row; // Scroll up
    if (cursor->row >= *line_offset + ws->ws_row) *line_offset = cursor->row - ws->ws_row + 1; // Scroll down
}

void render(RopeNode *root, Cursor *cursor, size_t *line_offset, struct winsize *ws, char **print_buf, size_t *buf_len, size_t *buf_cap) {
    clear_screen();
    Renderer_move_to(0, 0);

    size_t total_lines = rope_total_newlines(root);
    ioctl(STDOUT_FILENO, TIOCGWINSZ, ws); // get window size

    for (size_t i = 0; i < ws->ws_row; i++) {
        size_t line = i + *line_offset;
        if (line > total_lines) break;

        if (i > 0) write(STDOUT_FILENO, "\r\n", 2);

        size_t line_start = rope_offset_of_line_start(root, line);
        size_t line_end = line >= total_lines ? root->weight : rope_offset_of_line_start(root, line + 1) - 1;

        *buf_len = 0;
        rope_collect_between(root, line_start, line_end, print_buf, buf_len, buf_cap);
        Renderer_print_line_number(line + 1);

        Renderer_print_buf(*print_buf, *buf_len);
    }
}

int main(int argc, char** argv) {
    char *filename = argv[1];
    
    if (argc < 2) {
        die("File name required.");
    }

    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size

    RopeNode root;
    rope_init(&root);
    
    load_file(&root, argv[1]);

    char* print_buf = malloc(1024);
    size_t buf_len = 0;
    size_t buf_cap = 1024;

    Cursor cursor;
    cursor_init(&cursor);


    Renderer_Init();
    
    size_t line_offset = 0;
    size_t total_newlines = rope_total_newlines(&root);

    render(&root, &cursor, &line_offset, &ws, &print_buf, &buf_len, &buf_cap);
    
    size_t click_row = 0;
    size_t click_col = 0;

    while (1) {
        char buf[5];
        int r = read_key(buf, &click_row, &click_col);

        if (r == QUIT) {
            Renderer_Exit();
        } else if (r == SAVE) {
            save_file(&root, argv[1]);
        } else if (r == BACKSPACE) {
            cursor_backspace(&cursor, &root);
        } else if (r == ARROW_UP) {
            cursor_move_vertical(&cursor, &root, -1);
            scroll_to_cursor(&cursor, &line_offset, &ws);
        } else if (r == ARROW_DOWN) {
            cursor_move_vertical(&cursor, &root, 1);
            scroll_to_cursor(&cursor, &line_offset, &ws);
        } else if (r == ARROW_LEFT) {
            cursor_move_horisontal(&cursor, &root, -1);
            scroll_to_cursor(&cursor, &line_offset, &ws);
        } else if (r == ARROW_RIGHT) {
            cursor_move_horisontal(&cursor, &root, 1);
            scroll_to_cursor(&cursor, &line_offset, &ws);
        } else if (r == MOUSE_SCROLL_UP) {
            if (line_offset > 0) line_offset--;
        } else if (r == MOUSE_SCROLL_DOWN) {
            if (line_offset < total_newlines) line_offset++;
        } else if (r == MOUSE_CLICK) {
            size_t target_row = (click_row - 1) + line_offset;
            int target_col_raw = ((int) click_col - 1) - LEFT_MARGIN;
            size_t target_col = target_col_raw < 0 ? 0 : (size_t) target_col_raw;

            cursor_set_position(&cursor, &root, target_row, target_col);
            scroll_to_cursor(&cursor, &line_offset, &ws);
        } else if (r == '\x1b') {
            // bare Escape, mouse release, or any other unhandled escape
            // sequence — nothing to do, must not fall into text insertion
        } else {

            rope_insert(&root, cursor.offset, buf, r);
            cursor.offset += r;
            cursor_get_row_col(&cursor, &root);
            cursor.goal_column = cursor.column;
            scroll_to_cursor(&cursor, &line_offset, &ws);
        }

        // Render
        //
        total_newlines = rope_total_newlines(&root);
        
        render(&root, &cursor, &line_offset, &ws, &print_buf, &buf_len, &buf_cap);

        // buf_len = 0;
        // rope_collect(&root, &print_buf, &buf_len, &buf_cap);
        // cursor_get_row_col(&cursor, &root);
        

        // clear_screen();
        // char seq[32];
        // int len = snprintf(seq, sizeof(seq), "\x1b[%d;%dH", row, col);
        // write(STDOUT_FILENO, seq, len);
        // size_t total_rows = rope_total_newlines(&root);
        // Renderer_print_line_numbers(total_rows, 0);
        //
        // write(STDOUT_FILENO, "\x1b[H", 3);
        // Renderer_print_buf(print_buf, buf_len);
        print_cursor_debug(&cursor, &root);
        // move cursor to cursor pos
        Renderer_print_cursor(&cursor, line_offset, ws.ws_row);
    }
    return 0;
}
