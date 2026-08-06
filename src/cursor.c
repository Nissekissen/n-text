#include "cursor.h"
#include "rope.h"

void cursor_init(Cursor *cursor) {
    cursor->row = 0;
    cursor->column = 0;
    cursor->offset = 0;
}

void cursor_get_row_col(Cursor *cursor, RopeNode *rope) {
    size_t line = rope_line_of_offset(rope, cursor->offset);
    size_t col = cursor->offset - rope_offset_of_line_start(rope, line);

    cursor->row = line;
    cursor->column = col;
}

void cursor_move_vertical(Cursor *cursor, RopeNode *rope, int delta) {
    size_t total_lines = rope_total_newlines(rope);
    size_t line = rope_line_of_offset(rope, cursor->offset);
    size_t col = rope_offset_of_line_start(rope, line);

    if ((delta < 0 && line == 0) || (delta > 0 && line + 1 >= total_lines))
        return;

    size_t target_line = line + delta;
    size_t len = rope_line_length(rope, target_line, total_lines);
    col = 0;
    cursor->offset = rope_offset_of_line_start(rope, target_line) + col;
    cursor->row = target_line;
    cursor->column = col;
}
