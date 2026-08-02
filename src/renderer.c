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

void Renderer_Print(char c) {
    write(STDOUT_FILENO, &c, 1);
}

void Renderer_print_buf(char* buf, size_t buf_len) {
    for (int i = 0; i < buf_len; i++) {
        Renderer_Print(buf[i]);
    }
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
    return c;
}
