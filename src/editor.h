#pragma once

#include <sys/ioctl.h>
#include <stdint.h>

#include "rope.h"
#include "cursor.h"
#include "renderer.h"

typedef enum {
    MODE_NORMAL,
    MODE_PROMPT_SAVE
} editor_mode_t;

typedef struct {
    editor_mode_t mode;
    char   prompt_buf[256];
    size_t prompt_len;
    int dirty;
} status_bar_t;

typedef struct {
    int active;
    size_t anchor_offset;
    int via_toggle;
} selection_t;

typedef struct {
    rope_node_t root;
    cursor_t cursor;
    char *filename;
    size_t line_offset;
    struct winsize ws;
    status_bar_t status_bar;
    selection_t selection;

    char *clipboard;
    size_t clipboard_len;
    size_t clipboard_cap;
} editor_t;

void editor_init();
void editor_set_filename(const char *filename);
void editor_handle_event(input_event_t event);
void editor_render();

int visible_rows(editor_t *editor);
void print_with_highlight(char *buf, size_t len, size_t start, size_t end);
