#pragma once

#include <stdio.h>

#include "cursor.h"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_RIGHT,
    ARROW_LEFT
};

void Renderer_Init(void);
void Renderer_Exit(void);
void Renderer_Print(int c);
void Renderer_print_buf(char* buf, size_t buf_len);
void Renderer_print_cursor(Cursor* cursor);
void die(const char* s);
// Private methods
void enable_raw_mode(void);
void disable_raw_mode(void);

// Might move out of renderer sometime
int read_key(char *buf);
int utf8_seq_len(unsigned char lead);

void clear_screen(void);
