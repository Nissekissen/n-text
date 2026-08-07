#include "renderer.h"
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>


struct termios orig_termos;

void Renderer_Init(void) {

    write(STDOUT_FILENO, "\x1b[?1049h", 8); // Enter alt screen
    enable_raw_mode();

    write(STDOUT_FILENO, "\x1b[?1000h", 8); // enable mouse tracking
    write(STDOUT_FILENO, "\x1b[?1006h", 8); // SGR extended mode
    write(STDOUT_FILENO, "\x1b[?1002h", 8); // Enable mouse dragging
    write(STDOUT_FILENO, "\x1b[6 q", 5); // Set cursor shape
}

void Renderer_Exit(void) {

    write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to home
    
    write(STDOUT_FILENO, "\x1b[1 q", 5); // reset cursor
    
    write(STDOUT_FILENO, "\x1b[?1049l", 8); // back to original terminal

    write(STDOUT_FILENO, "\x1b[?1000l", 8); // disable mouse tracking
    write(STDOUT_FILENO, "\x1b[?1006l", 8); // disable SGR extended mode
    write(STDOUT_FILENO, "\x1b[?1002l", 8); // disbale mouse dragging
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

void Renderer_print_line_number(size_t number) {
    
    write(STDOUT_FILENO, "\033[38;5;242m", 11); // Set color to gray
    
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%4zu ", number);
    write(STDOUT_FILENO, buf, len);

    write(STDOUT_FILENO, "\033[0m", 4); // Reset color
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
    for (int i = 0; i < buf_len; i++) {
        Renderer_Print(buf[i]);
    }
}

void Renderer_print_cursor(Cursor *cursor, size_t line_offset, size_t visible_rows) {
    if (cursor->row < line_offset || cursor->row >= line_offset + visible_rows) {
        write(STDOUT_FILENO, "\x1b[?25l", 6); // hide cursor, it's scrolled out of view
        return;
    }
    write(STDOUT_FILENO, "\x1b[?25h", 6); // ensure visible
    Renderer_move_to((int) (cursor->row - line_offset) + 1 + 1, (int) cursor->column + 1 + LEFT_MARGIN);
}

void Renderer_move_right(size_t chars) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "\x1b[%zuC", chars);
    write(STDOUT_FILENO, buf, len);
}

void die(const char *s) {
    perror(s);
    Renderer_Exit();
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

int read_ascii_number(unsigned char *c) {
    int value = 0;
    while (read(STDIN_FILENO, c, 1) == 1 && *c >= '0' && *c <= '9') {
        value = value * 10 + (*c - '0');
    }

    return value;
}

int read_key(char *buf, size_t *click_row, size_t *click_col) {
    unsigned char c;
    ssize_t nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    if (c == '\r') { buf[0] = '\n'; return 1; };
    
    if (c == CTRL_KEY('s')) return SAVE;
    if (c == CTRL_KEY('S')) return SAVE_AS;
    if (c == CTRL_KEY('q')) return QUIT;
    if (c == 0x7F) return BACKSPACE;

    if (c == '\t') {
        for (int i = 0; i < TAB_WIDTH; i++) buf[i] = ' ';
        return TAB_WIDTH;
    }
    if (c == '\x1b') {
        char seq[12];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

        if (seq[0] == '[') {

            if (seq[1] == '<') {
                // Mouse scrolling / click — always consume the full
                // Cb;Cx;Cy(M|m) sequence before deciding what to do with it.
                int Cb = read_ascii_number(&c);
                int Cx = read_ascii_number(&c);
                int Cy = read_ascii_number(&c);
                // c now holds the terminator: 'M' = press, 'm' = release

                if (Cb == 64) return MOUSE_SCROLL_UP;
                if (Cb == 65) return MOUSE_SCROLL_DOWN;

                if (Cb == 0 && c == 'M') {
                    *click_row = Cy;
                    *click_col = Cx;
                    return MOUSE_CLICK;
                }

                return '\x1b'; // some other button/event we don't handle yet
            }

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

