#include "renderer.h"
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

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
    int len = snprintf(buf, 12, "\x1b[%d;%dH", (int)cursor->row + 1, (int)cursor->column + 1);
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

int read_key(char *buf) {
    unsigned char c;
    ssize_t nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    if (c == '\r') { buf[0] = '\n'; return 1; };


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
        
    }
    
    int len = utf8_seq_len(c);
    buf[0] = c;
    nread = read(STDIN_FILENO, buf + 1, len - 1);
    if (nread != len - 1) die("read");
    return len;
}

int utf8_seq_len(unsigned char lead) {
    if ((lead & 0x80) == 0x00) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

