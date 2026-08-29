#include "cursor.h"
#include "rope.h"
#include "renderer.h"

void cursor_init(Cursor *cursor) {
    cursor->row = 0;
    cursor->column = 0;
    cursor->goal_column = 0;
    cursor->offset = 0;
}

void cursor_get_row_col(Cursor *cursor, RopeNode *rope) {
    size_t line = rope_line_of_offset(rope, cursor->offset);
    size_t line_start = rope_offset_of_line_start(rope, line);

    cursor->row = line;
    cursor->column = rope_count_chars_between(rope, line_start, cursor->offset);
}

void cursor_move_vertical(Cursor *cursor, RopeNode *rope, int delta) {
    size_t total_lines = rope_total_newlines(rope);
    cursor_get_row_col(cursor, rope);

    if (delta < 0 && cursor->row == 0)
        return;
    
    size_t target_line = cursor->row + (cursor->row >= total_lines && delta > 0 ? 0 : delta);
    size_t len = rope_line_length(rope, target_line, total_lines);
    size_t col = cursor->goal_column > len ? len : cursor->goal_column;

    if (delta > 0 && cursor->row >= total_lines) {
        col = len;
    }

    size_t offset = rope_offset_of_line_start(rope, target_line);
    for (size_t i = 0; i < col; i++) {
        offset += move_forward_utf8(rope, offset);
    }

    cursor->offset = offset;
    cursor->row = target_line;
    cursor->column = col;
}

void cursor_move_horisontal(Cursor *cursor, RopeNode *rope, int delta) {
    size_t total_lines = rope_total_newlines(rope);
    cursor_get_row_col(cursor, rope);
    size_t line_length = rope_line_length(rope, cursor->row, total_lines);

    if ((delta < 0 && cursor->column == 0) || (delta > 0 && cursor->column >= line_length))
        return;
    
    if (delta > 0) {
        size_t len = move_forward_utf8(rope, cursor->offset);
        cursor->offset += len;
    } else {
        size_t back = move_back_utf8(rope, cursor->offset);
        cursor->offset -= back;
    }

    cursor_get_row_col(cursor, rope);
    cursor->goal_column = cursor->column;
}

void cursor_backspace(Cursor *cursor, RopeNode *rope) {
    if (cursor->offset == 0) return;

    size_t back = move_back_utf8(rope, cursor->offset);

    rope_delete(rope, cursor->offset - back, back);
    cursor->offset -= back;

    cursor_get_row_col(cursor, rope);
    cursor->goal_column = cursor->column;
}

void cursor_delete_section(Cursor *cursor, RopeNode *rope, size_t start, size_t end) {
    if (start > end) return;
    if (start == end) return cursor_backspace(cursor, rope);

    rope_delete(rope, start, end - start);

    cursor->offset = start;
    cursor_get_row_col(cursor, rope);
    cursor->goal_column = cursor->column;
}

void cursor_set_position(Cursor *cursor, RopeNode *rope, size_t target_row, size_t target_col) {
    size_t total_lines = rope_total_newlines(rope);
    if (target_row > total_lines) target_row = total_lines;

    size_t len = rope_line_length(rope, target_row, total_lines);
    size_t col = target_col > len ? len : target_col;

    size_t offset = rope_offset_of_line_start(rope, target_row);
    for (size_t i = 0; i < col; i++) {
        offset += move_forward_utf8(rope, offset);
    }

    cursor->offset = offset;
    cursor->row = target_row;
    cursor->column = col;
    cursor->goal_column = col;
}

size_t move_forward_utf8(RopeNode *rope, size_t offset) {
    char *buf = malloc(1);
    size_t buf_len = 0, buf_cap = 1;
    rope_collect_between(rope, offset, offset + 1, &buf, &buf_len, &buf_cap);

    int len = utf8_seq_len((unsigned char)buf[0]);
    free(buf);

    return len;
}

size_t move_back_utf8(RopeNode *rope, size_t offset) {
    size_t window_start = offset >= 4 ? offset - 4 : 0;
    char *buf = malloc(4);
    size_t buf_len = 0, buf_cap = 4;
    rope_collect_between(rope, window_start, offset, &buf, &buf_len, &buf_cap);

    size_t back = 1;
    while (back < buf_len && ((unsigned char)buf[buf_len - back] & 0xC0) == 0x80) {
        back++;
    }
    free(buf);

    return back;
}
