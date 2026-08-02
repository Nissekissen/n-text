#pragma once

#include <stdio.h>

#define CTRL_KEY(k) ((k) & 0x1f)

void Renderer_Init(void);
void Renderer_Exit(void);
void Renderer_Print(char c);
void Renderer_print_buf(char* buf, size_t buf_len);
void die(const char* s);
// Private methods
void enable_raw_mode(void);
void disable_raw_mode(void);

// Might move out of renderer sometime
int read_key(void);

void clear_screen(void);
