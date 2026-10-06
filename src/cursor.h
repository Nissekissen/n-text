#pragma once

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

#include "rope.h"

typedef struct {
    uint16_t offset;
    size_t row;
    size_t column;
    size_t goal_column;
} cursor_t;

void cursor_init(cursor_t *cursor);
void cursor_get_row_col(cursor_t* cursor, rope_node_t *rope);
void cursor_move_vertical(cursor_t *cursor, rope_node_t *rope, int delta);
void cursor_move_horisontal(cursor_t *cursor, rope_node_t *rope, int delta);
void cursor_backspace(cursor_t *cursor, rope_node_t *rope);
void cursor_delete_section(cursor_t *cursor, rope_node_t *rope, size_t start, size_t end);
void cursor_set_position(cursor_t *cursor, rope_node_t *rope, size_t target_row, size_t target_col);
size_t cursor_segment(cursor_t *cursor, size_t visible_width);

size_t move_forward_utf8(rope_node_t *rope, size_t offset);
size_t move_back_utf8(rope_node_t *rope, size_t offset);
