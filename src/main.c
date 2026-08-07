#include <stdio.h>
#include <stdlib.h>
#include "renderer.h"
#include <unistd.h>
#include "rope.h"
#include "cursor.h"
#include <sys/ioctl.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>

typedef struct {
    EditorMode mode;
    char **prompt_buf;
    size_t *prompt_len;
    int dirty;
} StatusBar;

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

    // Chunk size must stay well under LEAF_MAX_SIZE / 2: rope_insert only
    // ever performs one split per retry, and a single split roughly halves
    // a full leaf's existing content — a chunk close to LEAF_MAX_SIZE won't
    // fit after just one split, and splitting again never shrinks str_len,
    // so it recurses forever instead of converging.
    size_t chunk_start = 0;
    while (chunk_start < size) {
        size_t max_chunk = LEAF_MAX_SIZE / 4;
        size_t chunk_size = (size - chunk_start) >= max_chunk ? max_chunk : (size - chunk_start);
        rope_insert(root, chunk_start, buf + chunk_start, chunk_size);
        chunk_start += chunk_size;
    }

    free(buf);
}

void scroll_to_cursor(Cursor *cursor, size_t *line_offset, struct winsize *ws) {
    if (cursor->row < *line_offset) *line_offset = cursor->row; // Scroll up
    if (cursor->row >= *line_offset + ws->ws_row) *line_offset = cursor->row - ws->ws_row + 1; // Scroll down
}

void render(RopeNode *root, Cursor *cursor, StatusBar *status_bar, char *filename, size_t *line_offset, size_t *visible_rows, char **print_buf, size_t *buf_len, size_t *buf_cap) {
    clear_screen();
    
    // Print status bar
    char buf[300];
    size_t status_len;
    if (status_bar->mode == MODE_PROMPT_SAVE) {
        status_len = snprintf(buf, sizeof(buf), "Save as: ");
        size_t remaining = sizeof(buf) - status_len;
        size_t copy_len = *status_bar->prompt_len < remaining ? *status_bar->prompt_len : remaining;
        memcpy(buf + status_len, *status_bar->prompt_buf, copy_len);
        status_len += copy_len;
    } else if (filename) {
        status_len = snprintf(buf, sizeof(buf), "%s%s", filename, status_bar->dirty ? " *" : "");
    } else {
        status_len = snprintf(buf, sizeof(buf), "unsaved*");
    }

    Renderer_move_to(1, 2);
    Renderer_print_buf(buf, status_len);
    
    Renderer_move_to(2, 1);

    size_t total_lines = rope_total_newlines(root);

    for (size_t i = 0; i < *visible_rows; i++) {
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
    if (argc < 2) filename = NULL;
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size
    size_t visible_rows = ws.ws_row - 1;

    RopeNode root;
    rope_init(&root);
    
    if (filename != NULL) load_file(&root, argv[1]);

    char* print_buf = malloc(1024);
    size_t buf_len = 0;
    size_t buf_cap = 1024;

    Cursor cursor;
    cursor_init(&cursor);
 
    char *prompt_buf = malloc(256);
    size_t prompt_len = 0;

    StatusBar status_bar;
    status_bar.mode = MODE_NORMAL;
    status_bar.prompt_buf = &prompt_buf;
    status_bar.prompt_len = &prompt_len;
    status_bar.dirty = 0;


    Renderer_Init();
    
    size_t line_offset = 0;
    size_t total_newlines = rope_total_newlines(&root);

    render(&root, &cursor, &status_bar, filename, &line_offset, &visible_rows, &print_buf, &buf_len, &buf_cap);
    
    size_t click_row = 0;
    size_t click_col = 0;

    while (1) {
        char buf[5];
        int r = read_key(buf, &click_row, &click_col);
        
        if (status_bar.mode == MODE_PROMPT_SAVE) {
            if (r == 1 && buf[0] == '\n') { // Enter - confirm
                prompt_buf[prompt_len] = '\0';
                filename = strdup(prompt_buf);
                save_file(&root, filename);
                status_bar.dirty = 0;
                status_bar.mode = MODE_NORMAL;
            } else if (r == '\x1b') {
                status_bar.mode = MODE_NORMAL;
            } else if (r == BACKSPACE) {
                if (prompt_len > 0) prompt_len--;
            } else if (r < 1000) {
                memcpy(prompt_buf + prompt_len, buf, r);
                prompt_len += r;
            }
        } else {
            if (r == QUIT) {
                Renderer_Exit();
            } else if (r == SAVE) {
                if (filename != NULL) {
                    save_file(&root, filename);
                    status_bar.dirty = 0;
                } else {
                    status_bar.mode = MODE_PROMPT_SAVE;
                    prompt_len = 0;
                }
            } else if (r == SAVE_AS) {
                status_bar.mode = MODE_PROMPT_SAVE;
                prompt_len = 0;
            } else if (r == BACKSPACE) {
                cursor_backspace(&cursor, &root);
                status_bar.dirty = 1;
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
                size_t target_row = (click_row - 1) + line_offset - 1;
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
                status_bar.dirty = 1;
            }
        }

        // Render
        //
        total_newlines = rope_total_newlines(&root);
        
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size
        visible_rows = ws.ws_row - 1;

        render(&root, &cursor, &status_bar, filename, &line_offset, &visible_rows, &print_buf, &buf_len, &buf_cap);

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

    free(print_buf);
    free(prompt_buf);
    return 0;
}
