#include <stdio.h>
#include <stdlib.h>
#include "renderer.h"
#include <unistd.h>
#include "rope.h"
#include "cursor.h"
#include <sys/ioctl.h>

void print_cursor_debug(int row, int col) {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size

    char seq[64];
    int len = snprintf(seq, sizeof(seq),
        "\x1b""7"                 // DECSC: save cursor pos + attrs
        "\x1b[%d;%dH"              // move to bottom-right-ish corner
        "(%d,%d)"                  // the debug text
        "\x1b""8",                 // DECRC: restore cursor pos + attrs
        ws.ws_row, ws.ws_col - 10, row, col);

    write(STDOUT_FILENO, seq, len);
}

int main(void) {
    
    Renderer_Init();

    RopeNode root;
    rope_init(&root);
    
    char* print_buf = malloc(1024);
    size_t buf_len = 0;
    size_t buf_cap = 1024;

    Cursor cursor;
    cursor_init(&cursor);

    while (1) {
        int c = read_key();
        if (c == CTRL_KEY('q')) {
            Renderer_Exit();
        }

        if (c == ARROW_UP) {
            cursor_move_vertical(&cursor, &root, -1);
        } else if (c == ARROW_DOWN) {
            cursor_move_vertical(&cursor, &root, 1);
        } else {
            char s[2] = { c, '\0' };

            rope_insert(&root, cursor.offset, s);

            buf_len = 0;
            
            rope_collect(&root, &print_buf, &buf_len, &buf_cap);
            cursor.offset++;
            cursor_get_row_col(&cursor, &root);
        }

        clear_screen();
        // char seq[32];
        // int len = snprintf(seq, sizeof(seq), "\x1b[%d;%dH", row, col);
        // write(STDOUT_FILENO, seq, len);
        write(STDOUT_FILENO, "\x1b[H", 3);
        Renderer_print_buf(print_buf, buf_len);
        print_cursor_debug(cursor.row, cursor.column);
        // move cursor to cursor pos
        char buf[12];
        int len = snprintf(buf, 12, "\x1b[%d;%dH", (int)cursor.row + 1, (int)cursor.column);
        write(STDOUT_FILENO, buf, len);
    }
    return 0;
}
