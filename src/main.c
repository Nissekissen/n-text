#include <stdio.h>
#include <stdlib.h>
#include "renderer.h"
#include <unistd.h>
#include "rope.h"


int main(void) {
    
    Renderer_Init();

    RopeNode root;
    rope_init(&root);
    
    char* print_buf = malloc(1024);
    size_t buf_len = 0;
    size_t buf_cap = 1024;

    uint16_t cursor_index = 0;

    while (1) {
        char c = read_key();
        if (c == CTRL_KEY('q')) {
            Renderer_Exit();
        }

        char s[2] = { c, '\0' };

        rope_insert(&root, cursor_index, s);

        buf_len = 0;
        
        rope_collect(&root, &print_buf, &buf_len, &buf_cap);
        cursor_index++;

        clear_screen();
        // char seq[32];
        // int len = snprintf(seq, sizeof(seq), "\x1b[%d;%dH", row, col);
        // write(STDOUT_FILENO, seq, len);
        write(STDOUT_FILENO, "\x1b[H", 3);
        Renderer_print_buf(print_buf, buf_len);
        // move cursor to home
    }
    return 0;
}
