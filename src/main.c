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
    char   prompt_buf[256];
    size_t prompt_len;
    int dirty;
} StatusBar;

typedef struct {
    RopeNode root;
    Cursor cursor;
    char *filename;
    size_t line_offset;
    struct winsize ws;
    StatusBar status_bar;
} Editor;

int visible_rows(Editor *editor) {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &editor->ws);
    return editor->ws.ws_row - 1;
}

void print_cursor_debug(Editor *editor) {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size
    size_t total_newlines = rope_total_newlines(&editor->root);
    char seq[64];
    int len = snprintf(seq, sizeof(seq),
        "\x1b""7"                 // DECSC: save cursor pos + attrs
        "\x1b[%d;%dH"              // move to bottom-right-ish corner
        "(%d,%d) %d, %d"                  // the debug text
        "\x1b""8",                 // DECRC: restore cursor pos + attrs
        editor->ws.ws_row, editor->ws.ws_col - 20, (int)editor->cursor.row, (int)editor->cursor.column, (int)total_newlines, (int)editor->cursor.offset);

    write(STDOUT_FILENO, seq, len);
}

void save_file(Editor *editor, char *dst) {
    char *buf = malloc(1024);
    size_t buf_len = 0, buf_cap = 1024;
    rope_collect(&editor->root, &buf, &buf_len, &buf_cap);

    if (buf_len == 0 || buf[buf_len - 1] != '\n') {
        safe_append(&buf, &buf_len, &buf_cap, "\n", 1);
    }

    int fd = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) return; // TODO! add error handling here
    write(fd, buf, buf_len);
    close(fd);

    free(buf);
}

void load_file(Editor *editor, char *path) {
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
        rope_insert(&editor->root, chunk_start, buf + chunk_start, chunk_size);
        chunk_start += chunk_size;
    }

    free(buf);
}

void handle_arrow_keys(Editor *editor, int arrow_key, int shift) {
    if (arrow_key == ARROW_UP || arrow_key == ARROW_DOWN) {
        int delta = arrow_key == ARROW_UP ? -1 : 1;
        cursor_move_vertical(&editor->cursor, &editor->root, delta);
    } else {
        int delta = arrow_key == ARROW_LEFT ? -1 : 1;
        cursor_move_horisontal(&editor->cursor, &editor->root, delta);
    }

    scroll_to_cursor(&editor->cursor, &editor->line_offset, &editor->ws);
}

void handle_prompt_mode(Editor *editor, InputEvent event) {
    if (event.byte_count == 1 && event.bytes[0] == '\n') {
        editor->status_bar.prompt_buf[editor->status_bar.prompt_len] = '\0';
        editor->filename = strdup(editor->status_bar.prompt_buf);
        save_file(editor, editor->filename);
        editor->status_bar.dirty = 0;
        editor->status_bar.mode = MODE_NORMAL;

        return;
    }

    if (event.byte_count > 0) {
        memcpy(editor->status_bar.prompt_buf + editor->status_bar.prompt_len, event.bytes, event.byte_count);
        editor->status_bar.prompt_len += event.byte_count;
    }

    if (event.keyCode == 0) return;

    switch (event.keyCode) {
        case BACKSPACE:
            if (editor->status_bar.prompt_len > 0) editor->status_bar.prompt_len--;
            break;
        case ESC:
            editor->status_bar.mode = MODE_NORMAL;
            break;
    }
}

void render(Editor *editor) {
    clear_screen();

    int _visible_rows = visible_rows(editor);
    static char *print_buf = NULL;
    static size_t buf_len = 0, buf_cap = 0;
    
    // Print status bar
    char buf[300];
    size_t status_len;
    if (editor->status_bar.mode == MODE_PROMPT_SAVE) {
        status_len = snprintf(buf, sizeof(buf), "Save as: ");
        size_t remaining = sizeof(buf) - status_len;
        size_t copy_len = editor->status_bar.prompt_len < remaining ? editor->status_bar.prompt_len : remaining;
        memcpy(buf + status_len, editor->status_bar.prompt_buf, copy_len);
        status_len += copy_len;
    } else if (editor->filename) {
        status_len = snprintf(buf, sizeof(buf), "%s%s", editor->filename, editor->status_bar.dirty ? " *" : "");
    } else {
        status_len = snprintf(buf, sizeof(buf), "unsaved*");
    }

    Renderer_move_to(1, 2);
    Renderer_print_buf(buf, status_len);
    
    Renderer_move_to(2, 1);

    size_t total_lines = rope_total_newlines(&editor->root);

    for (size_t i = 0; i < _visible_rows; i++) {
        size_t line = i + editor->line_offset;
        if (line > total_lines) break;

        if (i > 0) write(STDOUT_FILENO, "\r\n", 2);

        size_t line_start = rope_offset_of_line_start(&editor->root, line);
        size_t line_end = line >= total_lines ? editor->root.weight : rope_offset_of_line_start(&editor->root, line + 1) - 1;

        buf_len = 0;
        rope_collect_between(&editor->root, line_start, line_end, &print_buf, &buf_len, &buf_cap);
        Renderer_print_line_number(line + 1);

        Renderer_print_buf(print_buf, buf_len);
    }
    
    print_cursor_debug(editor);
    Renderer_print_cursor(&editor->cursor, editor->line_offset, _visible_rows);
}

int main(int argc, char** argv) {
    Editor editor = {0};
    
    editor.filename = argv[1];
    if (argc < 2) editor.filename = NULL;

    rope_init(&editor.root);
    if (editor.filename != NULL) load_file(&editor, editor.filename);

    cursor_init(&editor.cursor);
    editor.status_bar.mode = MODE_NORMAL;
    editor.status_bar.dirty = 0;


    Renderer_Init();

    render(&editor); 

    while (1) {
        size_t total_newlines = rope_total_newlines(&editor.root);
        InputEvent event = read_key();

        if (editor.status_bar.mode == MODE_PROMPT_SAVE) {
            handle_prompt_mode(&editor, event);
            render(&editor);
            continue;
        }

        if (event.byte_count > 0) {
            rope_insert(&editor.root, editor.cursor.offset, event.bytes, event.byte_count);
            editor.cursor.offset += event.byte_count;
            cursor_get_row_col(&editor.cursor, &editor.root);
            editor.cursor.goal_column = editor.cursor.column;
            scroll_to_cursor(&editor.cursor, &editor.line_offset, &editor.ws);
            editor.status_bar.dirty = 1;

            render(&editor);
            continue;
        }

        if (event.keyCode == 0) continue;

        switch (event.keyCode) {
            case QUIT: 
                Renderer_Exit();
                break;
            case SAVE:
                if (editor.filename != NULL) {
                    save_file(&editor, editor.filename);
                    editor.status_bar.dirty = 0;
                } else {
                    editor.status_bar.mode = MODE_PROMPT_SAVE;
                    editor.status_bar.prompt_len = 0;
                }
                break;
            case SAVE_AS: 
                editor.status_bar.mode = MODE_PROMPT_SAVE;
                editor.status_bar.prompt_len = 0;
                break;
            case BACKSPACE:
                cursor_backspace(&editor.cursor, &editor.root);
                editor.status_bar.dirty = 1;
                break;
            case ARROW_UP:
            case ARROW_DOWN:
            case ARROW_LEFT:
            case ARROW_RIGHT:
                handle_arrow_keys(&editor, event.keyCode, event.modifiers & MOD_SHIFT);
                break;
            case MOUSE_SCROLL_UP:
                if (editor.line_offset > 0) editor.line_offset--;
                break;
            case MOUSE_SCROLL_DOWN:
                if (editor.line_offset < total_newlines) editor.line_offset++;
                break;
            case MOUSE_CLICK: {
                size_t target_row = (event.click_row - 1) + editor.line_offset - 1;
                int target_col_raw = ((int) event.click_col - 1) - LEFT_MARGIN;
                size_t target_col = target_col_raw < 0 ? 0 : (size_t) target_col_raw;

                cursor_set_position(&editor.cursor, &editor.root, target_row, target_col);
                scroll_to_cursor(&editor.cursor, &editor.line_offset, &editor.ws);
                break;
            }
        }

        render(&editor);
    }

    return 0;
}
