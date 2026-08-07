#include <stdio.h>
#include <stdlib.h>
#include "renderer.h"
#include <unistd.h>
#include "rope.h"
#include "cursor.h"
#include <sys/ioctl.h>

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

int main(void) {
    
    Renderer_Init();

    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size

    RopeNode root;
    rope_init(&root);
    
    char* print_buf = malloc(1024);
    size_t buf_len = 0;
    size_t buf_cap = 1024;

    Cursor cursor;
    cursor_init(&cursor);

    size_t line_offset = 0;

    while (1) {
        char buf[5];
        int r = read_key(buf);

        if (r == QUIT) {
            Renderer_Exit();
        } else if (r == BACKSPACE) {
            cursor_backspace(&cursor, &root);
        } else if (r == ARROW_UP) {
            cursor_move_vertical(&cursor, &root, -1);
        } else if (r == ARROW_DOWN) {
            cursor_move_vertical(&cursor, &root, 1);
        } else if (r == ARROW_LEFT) {
            cursor_move_horisontal(&cursor, &root, -1);
        } else if (r == ARROW_RIGHT) {
            cursor_move_horisontal(&cursor, &root, 1);
        } else {

            rope_insert(&root, cursor.offset, buf, r);
            cursor.offset += r;
            cursor_get_row_col(&cursor, &root);
            cursor.goal_column = cursor.column;
        }

        // Render
        clear_screen();
        Renderer_move_to(0, 0);

        size_t total_lines = rope_total_newlines(&root);
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // get window size
        
        if (cursor.row < line_offset) line_offset = cursor.row; // Scroll up
        if (cursor.row >= line_offset + ws.ws_row) line_offset = cursor.row - ws.ws_row + 1; // Scroll down


        for (size_t i = 0; i < ws.ws_row; i++) {
            size_t line = i + line_offset;
            if (line > total_lines) break;

            if (i > 0) write(STDOUT_FILENO, "\r\n", 2);

            size_t line_start = rope_offset_of_line_start(&root, line);
            size_t line_end = line >= total_lines ? root.weight : rope_offset_of_line_start(&root, line + 1) - 1;

            buf_len = 0;
            rope_collect_between(&root, line_start, line_end, &print_buf, &buf_len, &buf_cap);
            Renderer_print_line_number(line + 1);

            Renderer_print_buf(print_buf, buf_len);
        }


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
        Renderer_print_cursor(&cursor, line_offset);
    }
    return 0;
}
