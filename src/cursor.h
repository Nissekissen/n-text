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
} Cursor;

void cursor_init(Cursor *cursor);
void cursor_get_row_col(Cursor* cursor, RopeNode *rope);
void cursor_move_vertical(Cursor *cursor, RopeNode *rope, int delta);
void cursor_move_horisontal(Cursor *cursor, RopeNode *rope, int delta);
void cursor_backspace(Cursor *cursor, RopeNode *rope);
void cursor_set_position(Cursor *cursor, RopeNode *rope, size_t target_row, size_t target_col);

size_t move_forward_utf8(RopeNode *rope, size_t offset);
size_t move_back_utf8(RopeNode *rope, size_t offset);
