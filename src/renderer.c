#include "renderer.h"
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

struct termios orig_termos;

void Renderer_Init(void) {
    enable_raw_mode();

    clear_screen();
    // Future rendering stuff maybe
    
}

void Renderer_Exit(void) {

    write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to home

    exit(0);
}

void Renderer_Print(int c) {
    if (c == '\n') {
        write(STDOUT_FILENO, "\r\n", 2);
        return;
    }
    write(STDOUT_FILENO, &c, 1);
}

void Renderer_print_buf(char* buf, size_t buf_len) {
    for (int i = 0; i < buf_len; i++) {
        Renderer_Print(buf[i]);
    }
}

void Renderer_print_cursor(Cursor *cursor) {
    char buf[12];
    int len = snprintf(buf, 12, "\x1b[%d;%dH", (int)cursor->row + 1, (int)cursor->column);
    write(STDOUT_FILENO, buf, len);
}

void die(const char *s) {
    clear_screen();
    write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to home
    perror(s);
    exit(0);
}

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termos);
}

void clear_screen(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termos);
    atexit(disable_raw_mode);

    struct termios raw = orig_termos;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int read_key(void) {
    char c;
    ssize_t nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == 1 && errno != EAGAIN) die("read");
    }

    if (c == '\r') return '\n';

    if (c == '\x1b') {
        char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {
            switch(seq[1]) {
                case 'A': return ARROW_UP;
                case 'B': return ARROW_DOWN;
                case 'C': return ARROW_RIGHT;
                case 'D': return ARROW_LEFT;
            }
        }
        
        return '\x1b';
    }
    
    return c;
}
