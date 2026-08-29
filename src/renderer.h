#pragma once

#include <stdio.h>
#include <sys/ioctl.h>
#include "cursor.h"

#define CTRL_KEY(k) ((k) & 0x1f)

#define MOD_SHIFT          (1 << 0)
#define MOD_ALT            (1 << 1)
#define MOD_CTRL           (1 << 2)
#define MOD_META           (1 << 3)

#define ESC '\x1b'

enum editorKey {
    ARROW_UP = 1000,
    ARROW_DOWN,
    ARROW_RIGHT,
    ARROW_LEFT,
    QUIT,
    SAVE,
    SAVE_AS,
    BACKSPACE,
    MOUSE_SCROLL_UP,
    MOUSE_SCROLL_DOWN,
    MOUSE_CLICK,
    MOUSE_DRAG,
    MOUSE_RELEASE,
    TOGGLE_SELECT,
    FORWARD_DELETE
};

typedef enum {
    MODE_NORMAL,
    MODE_PROMPT_SAVE
} EditorMode;

typedef struct {
    unsigned int parameterCount;
    unsigned int parameters[4];
    int privateMarker; // The leading < or ? after the [
    int finalChar;
} CSI_Parser_return;

typedef struct {
    int keyCode;
    char bytes[5];
    size_t byte_count;
    size_t click_row;
    size_t click_col;
    int modifiers;
} InputEvent;

void Renderer_Init(void);
void Renderer_Exit(void);
void Renderer_print_line_numbers(size_t total_lines, size_t start_line);
void Renderer_print_line_number(size_t number);
void Renderer_move_to(int row, int col);
void Renderer_move_right(size_t chars);
void Renderer_Print(int c);
void Renderer_print_buf(char* buf, size_t buf_len);
void Renderer_print_cursor(Cursor* cursor, size_t line_offset, size_t visible_rows);

void scroll_to_cursor(Cursor *cursor, size_t *line_offset, struct winsize *ws);

void die(const char* s);
// Private methods
void enable_raw_mode(void);
void disable_raw_mode(void);

// Might move out of renderer sometime
int read_ascii_number(unsigned char *c);
CSI_Parser_return csi_parse();
InputEvent read_key(void);
int utf8_seq_len(unsigned char lead);

void clear_screen(void);
