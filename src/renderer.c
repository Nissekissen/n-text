#include "renderer.h"
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#define TAB_WIDTH 4
#define LEFT_MARGIN 5

struct termios orig_termos;

void Renderer_Init(void) {

    write(STDOUT_FILENO, "\x1b[?1049h", 8); // Enter alt screen
    enable_raw_mode();

    clear_screen();
    // Future rendering stuff maybe

    // Set cursor shape
    write(STDOUT_FILENO, "\x1b[6 q", 5);   
}

void Renderer_Exit(void) {

    write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to home
    
    // Reset cursor shape
    write(STDOUT_FILENO, "\x1b[1 q", 5);
    
    write(STDOUT_FILENO, "\x1b[?1049l", 8); // back to original terminal

    exit(0);
}

void Renderer_print_line_numbers(size_t total_lines, size_t start_line) {
    
    // Set color to gray
    write(STDOUT_FILENO, "\033[38;5;242m", 11);
    for (size_t i = 0; i <= total_lines; i++) {
        // Move to start of line
        Renderer_move_to(i + 1, 0);
        
        char buf[8];
        int len = snprintf(buf, sizeof(buf), "%4zu", i + start_line + 1);
        write(STDOUT_FILENO, buf, len);
        
    }

    // Reset color
    write(STDOUT_FILENO, "\033[0m", 4);
}

void Renderer_move_to(int row, int col) {
    char buf[12];
    int len = snprintf(buf, 12, "\x1b[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, len);
}

void Renderer_Print(int c) {
    if (c == '\n') {
        write(STDOUT_FILENO, "\r\n", 2);

        // Move LEFT_MARGIN characters to the right
        Renderer_move_right(LEFT_MARGIN);
        return;
    }
    write(STDOUT_FILENO, &c, 1);
}

void Renderer_print_buf(char* buf, size_t buf_len) {
    // Move to 0, 0
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    // Move 4 characters to the right
    Renderer_move_right(LEFT_MARGIN);

    // Print the buffer
    for (int i = 0; i < buf_len; i++) {
        Renderer_Print(buf[i]);
    }
}

void Renderer_print_cursor(Cursor *cursor) {
    Renderer_move_to((int) cursor->row + 1, (int) cursor->column + 1 + LEFT_MARGIN);
}

void Renderer_move_right(size_t chars) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "\x1b[%zuC", chars);
    write(STDOUT_FILENO, buf, len);
}

void die(const char *s) {
    clear_screen();
    write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to home
    
    write(STDOUT_FILENO, "\x1b[?1049l", 8); // exit alt screen

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
    
    if (c == CTRL_KEY('q')) return QUIT;
    if (c == 0x7F) return BACKSPACE;

    if (c == '\t') {
        for (int i = 0; i < TAB_WIDTH; i++) buf[i] = ' ';
        return TAB_WIDTH;
    }
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

