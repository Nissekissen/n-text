#pragma once

#include <stdio.h>

#include "cursor.h"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_RIGHT,
    ARROW_LEFT,
    QUIT,
    SAVE,
    BACKSPACE,
    MOUSE_SCROLL_UP,
    MOUSE_SCROLL_DOWN,
    MOUSE_CLICK
};

void Renderer_Init(void);
void Renderer_Exit(void);
void Renderer_print_line_numbers(size_t total_lines, size_t start_line);
void Renderer_print_line_number(size_t number);
void Renderer_move_to(int row, int col);
void Renderer_move_right(size_t chars);
void Renderer_Print(int c);
void Renderer_print_buf(char* buf, size_t buf_len);
void Renderer_print_cursor(Cursor* cursor, size_t line_offset, size_t visible_rows);
void die(const char* s);
// Private methods
void enable_raw_mode(void);
void disable_raw_mode(void);

// Might move out of renderer sometime
int read_ascii_number(unsigned char *c);
int read_key(char *buf, size_t *click_row, size_t *click_col);
int utf8_seq_len(unsigned char lead);

void clear_screen(void);
