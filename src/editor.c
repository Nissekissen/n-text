#include "editor.h"
#include "renderer.h"
#include "file.h"

#include <string.h>
#include <stdbool.h>

static editor_t editor = {0};

void selection_range(size_t *start, size_t *end) {
    *start = min_size(editor.cursor.offset, editor.selection.anchor_offset);
    *start = max_size(editor.cursor.offset, editor.selection.anchor_offset);
}

void editor_init(void) {
    rope_init(&editor.root);
    if (editor.filename != NULL) load_file(&editor, editor.filename);

    cursor_init(&editor.cursor);
    editor.status_bar.mode = MODE_NORMAL;
    editor.status_bar.dirty = 0;

    Renderer_Init();
}

void editor_set_filename(const char *filename) {
    editor.filename = strdup(filename);
}

static void handle_arrow_keys(int arrow_key, int shift) {
    if (shift && !editor.selection.active) {
        editor.selection.anchor_offset = editor.cursor.offset;
        editor.selection.active = 1;
    }

    if (!shift && editor.selection.active && !editor.selection.via_toggle) {
        editor.selection.active = 0;
    }

    if (arrow_key == ARROW_UP || arrow_key == ARROW_DOWN) {
        int delta = arrow_key == ARROW_UP ? -1 : 1;
        cursor_move_vertical(&editor.cursor, &editor.root, delta);
    } else {
        int delta = arrow_key == ARROW_LEFT ? -1 : 1;
        cursor_move_horisontal(&editor.cursor, &editor.root, delta);
    }

    scroll_to_cursor(&editor.cursor, &editor.root, &editor.line_offset, visible_rows(&editor), editor.ws.ws_col - LEFT_MARGIN);
}

static void handle_prompt_mode(input_event_t event) {
    if (event.byte_count == 1 && event.bytes[0] == '\n') {
        editor.status_bar.prompt_buf[editor.status_bar.prompt_len] = '\0';
        editor.filename = strdup(editor.status_bar.prompt_buf);
        save_file(&editor, editor.filename);
        editor.status_bar.dirty = 0;
        editor.status_bar.mode = MODE_NORMAL;

        return;
    }

    if (event.byte_count > 0) {
        memcpy(editor.status_bar.prompt_buf + editor.status_bar.prompt_len, event.bytes, event.byte_count);
        editor.status_bar.prompt_len += event.byte_count;
    }

    if (event.keyCode == 0) return;

    switch (event.keyCode) {
        case BACKSPACE:
            if (editor.status_bar.prompt_len > 0) editor.status_bar.prompt_len--;
            break;
        case ESC:
            editor.status_bar.mode = MODE_NORMAL;
            break;
    }
}

void editor_insert_text(char *buf, size_t len) {
    rope_insert(&editor.root, editor.cursor.offset, buf, len);

    editor.cursor.offset += len;
    cursor_get_row_col(&editor.cursor, &editor.root);
    editor.cursor.goal_column = editor.cursor.column;
    scroll_to_cursor(&editor.cursor, &editor.root, &editor.line_offset, visible_rows(&editor), editor.ws.ws_col - LEFT_MARGIN);

    editor.status_bar.dirty = 1;
}

static void editor_save() {
    if (editor.filename != NULL) {
        save_file(&editor, editor.filename);
        editor.status_bar.dirty = 0;
        return;
    }

    editor.status_bar.mode = MODE_PROMPT_SAVE;
    editor.status_bar.prompt_len = 0;
}

static void editor_save_as() {
    editor.status_bar.mode = MODE_PROMPT_SAVE;
    editor.status_bar.prompt_len = 0;
}

static void editor_delete(bool is_forward_delete) {
    if (!editor.selection.active) {
        if (is_forward_delete) {
            if (editor.cursor.offset >= rope_total_length(&editor.root)) return;
            size_t len = move_forward_utf8(&editor.root, editor.cursor.offset);
            editor.cursor.offset += len;
        }
        
        cursor_backspace(&editor.cursor, &editor.root);
    }

    if (editor.selection.active) {
        size_t start = min_size(editor.cursor.offset, editor.selection.anchor_offset);
        size_t end   = max_size(editor.cursor.offset, editor.selection.anchor_offset);
        cursor_delete_section(&editor.cursor, &editor.root, start, end);
    }

    editor.status_bar.dirty = 1;
    editor.selection.active = 0;
    editor.selection.via_toggle = 0;
}

static void editor_copy() {
    if (!editor.selection.active) return;

    size_t sel_start, sel_end;
    selection_range(&sel_start, &sel_end);

    editor.clipboard_len = 0;
    rope_collect_between(&editor.root, sel_start, sel_end, &editor.clipboard, &editor.clipboard_len, &editor.clipboard_cap);
    Renderer_set_clipboard(editor.clipboard, editor.clipboard_len);
}

static void editor_cut() {
    if (!editor.selection.active) return;

    size_t sel_start, sel_end;
    selection_range(&sel_start, &sel_end);

    editor_copy();
    cursor_delete_section(&editor.cursor, &editor.root, sel_start, sel_end);

    editor.selection.active = 0;
    editor.selection.via_toggle = 0;
    editor.status_bar.dirty = 1;
}

static void editor_paste() {
    if (editor.clipboard_len == 0) return;

    if (editor.selection.active) {
        size_t sel_start, sel_end;
        selection_range(&sel_start, &sel_end);

        cursor_delete_section(&editor.cursor, &editor.root, sel_start, sel_end);
        editor.selection.active = 0;
        editor.selection.via_toggle = 0;
    }

    char *os_clipboard = Renderer_get_clipboard();
    if (os_clipboard != NULL) {
        editor.clipboard_len = 0;
        safe_append(&editor.clipboard, &editor.clipboard_len, &editor.clipboard_cap, os_clipboard, strlen(os_clipboard));
    }

    editor_insert_text(editor.clipboard, editor.clipboard_len);
}

static void editor_scroll(int dir) {
    size_t total_newlines = rope_total_newlines(&editor.root);

    if (dir < 0 && editor.line_offset > 0) editor.line_offset--;
    if (dir > 0 && editor.line_offset < total_newlines) editor.line_offset++;
}

static void editor_click(input_event_t event) {
    size_t total_newlines = rope_total_newlines(&editor.root);
    size_t visible_width = editor.ws.ws_col - LEFT_MARGIN;
    size_t target_visual_row = event.click_row - 2;

    line_segment_t loc = rope_line_of_visual_row(&editor.root, editor.line_offset, target_visual_row, editor.ws.ws_col - LEFT_MARGIN, total_newlines);
    int target_col_raw = ((int) event.click_col - 1) - LEFT_MARGIN;
    target_col_raw = target_col_raw < 0 ? 0 : (size_t) target_col_raw;

    size_t target_col = loc.segment * visible_width + target_col_raw;

    cursor_set_position(&editor.cursor, &editor.root, loc.line, target_col);
    scroll_to_cursor(&editor.cursor, &editor.root, &editor.line_offset, visible_rows(&editor), editor.ws.ws_col - LEFT_MARGIN);

    editor.selection.anchor_offset = editor.cursor.offset;
    editor.selection.active = 0;
    editor.selection.via_toggle = 0;
}

static void editor_drag(input_event_t event) {
    size_t target_row = (event.click_row - 1) + editor.line_offset - 1;
    int target_col_raw = ((int) event.click_col - 1) - LEFT_MARGIN;
    size_t target_col = target_col_raw < 0 ? 0 : (size_t) target_col_raw;

    cursor_set_position(&editor.cursor, &editor.root, target_row, target_col);
    scroll_to_cursor(&editor.cursor, &editor.root, &editor.line_offset, visible_rows(&editor), editor.ws.ws_col - LEFT_MARGIN);

    editor.selection.active = 1;
}

static void editor_toggle_select() {
    if (editor.selection.via_toggle) {
        editor.selection.active = 0;
        editor.selection.via_toggle = 0;
        return;
    }
    
    if (!editor.selection.active) editor.selection.anchor_offset = editor.cursor.offset;
    editor.selection.active = 1;
    editor.selection.via_toggle = 1;
}

void editor_handle_event(input_event_t event) {
    if (editor.status_bar.mode == MODE_PROMPT_SAVE) {
        handle_prompt_mode(event);
        editor_render();
        return;
    }

    if (event.byte_count > 0) {
        editor_insert_text(event.bytes, event.byte_count);
        editor.selection.active = 0;
        editor.selection.via_toggle = 0;

        editor_render();
        return;
    }

    if (event.paste_data != NULL) {
        editor_insert_text(event.paste_data, event.paste_len);
        free(event.paste_data);

        editor.selection.active = 0;
        editor.selection.via_toggle = 0;

        editor_render();
        return;
    }

    if (event.keyCode == 0) return;

    switch (event.keyCode) {
        case QUIT: 
            Renderer_Exit();
            break;
        case SAVE:
            editor_save();
            break;
        case SAVE_AS: 
            editor_save_as();
            break;
        case FORWARD_DELETE:
            editor_delete(true);
            break;
        case BACKSPACE:
            editor_delete(false);
            break;
        case CUT:
            editor_cut();
            break;
        case COPY:
            editor_copy();
            break;
        case PASTE:
            editor_paste();
            break;
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            handle_arrow_keys(event.keyCode, event.modifiers & MOD_SHIFT);
            break;
        case MOUSE_SCROLL_UP:
            editor_scroll(-1);
            break;
        case MOUSE_SCROLL_DOWN:
            editor_scroll(1);
            break;
        case MOUSE_CLICK:
            editor_click(event);
            break;
        case MOUSE_DRAG:
            editor_drag(event);
            break;
        case TOGGLE_SELECT:
            break;
    }
}

void editor_render(void) {
    clear_screen();

    int _visible_rows = visible_rows(&editor);
    static char *print_buf = NULL;
    static size_t buf_len = 0, buf_cap = 0;
    
    // Print status bar
    char buf[300];
    size_t status_len;
    if (editor.status_bar.mode == MODE_PROMPT_SAVE) {
        status_len = snprintf(buf, sizeof(buf), "Save as: ");
        size_t remaining = sizeof(buf) - status_len;
        size_t copy_len = editor.status_bar.prompt_len < remaining ? editor.status_bar.prompt_len : remaining;
        memcpy(buf + status_len, editor.status_bar.prompt_buf, copy_len);
        status_len += copy_len;
    } else if (editor.filename) {
        status_len = snprintf(buf, sizeof(buf), "%s%s", editor.filename, editor.status_bar.dirty ? " *" : "");
    } else {
        status_len = snprintf(buf, sizeof(buf), "unsaved*");
    }

    Renderer_move_to(1, 2);
    Renderer_print_buf(buf, status_len);
    
    Renderer_move_to(2, 1);

    size_t total_lines = rope_total_newlines(&editor.root);

    size_t sel_start = min_size(editor.cursor.offset, editor.selection.anchor_offset);
    size_t sel_end   = max_size(editor.cursor.offset, editor.selection.anchor_offset);
    
    size_t visible_width = editor.ws.ws_col - LEFT_MARGIN;

    size_t line = editor.line_offset;
    size_t rows_printed = 0;
    
    int _break = 0;

    while (rows_printed < _visible_rows) {
        if (line > total_lines) break;

        size_t line_start = rope_offset_of_line_start(&editor.root, line);
        size_t line_end = line >= total_lines ? rope_total_length(&editor.root) : rope_offset_of_line_start(&editor.root, line + 1) - 1;

        size_t overlap_start = max_size(line_start, sel_start);
        size_t overlap_end   = min_size(line_end, sel_end);

        size_t local_start = overlap_start - line_start;
        size_t local_end   = overlap_end   - line_start;

        buf_len = 0;
        rope_collect_between(&editor.root, line_start, line_end, &print_buf, &buf_len, &buf_cap);

        size_t byte_pos = 0;

        size_t segment_count = rope_segment_count(&editor.root, line, visible_width, total_lines);
        for (size_t seg = 0; seg < segment_count; seg++) {
            if (rows_printed >= _visible_rows) { _break = 1; break; }
            if (rows_printed > 0) write(STDOUT_FILENO, "\r\n", 2);
            
            size_t seg_start = byte_pos;

            for (size_t i = 0; i < visible_width; i++) {
                if (byte_pos >= buf_len) break;
                byte_pos += utf8_seq_len(print_buf[byte_pos]);
            }

            size_t seg_end = byte_pos;

            size_t seg_overlap_start = max_size(seg_start, local_start);
            size_t seg_overlap_end   = min_size(seg_end, local_end);

            if (seg == 0)
                Renderer_print_line_number(line + 1);
            else
                Renderer_move_right(LEFT_MARGIN);

            if (seg_overlap_start >= seg_overlap_end || !editor.selection.active) {
                Renderer_print_buf(print_buf + seg_start, seg_end - seg_start);
                rows_printed++;
                continue;
            }

            size_t seg_hl_start = seg_overlap_start - seg_start;
            size_t seg_hl_end   = seg_overlap_end   - seg_start;

            print_with_highlight(print_buf + seg_start, seg_end - seg_start, seg_hl_start, seg_hl_end);

            rows_printed++;
        }

        line++;

        if (_break) break;
    }

    // print_cursor_debug(editor);
    Renderer_print_cursor(&editor.cursor, &editor.root, editor.line_offset, _visible_rows, visible_width);
}

void print_with_highlight(char *buf, size_t len, size_t start, size_t end) {
    Renderer_print_buf(buf, start);
    Renderer_print_buf("\x1b[7m", 4);
    Renderer_print_buf(buf + start, end - start);
    Renderer_print_buf("\x1b[0m", 4);
    Renderer_print_buf(buf + end, len - end);
}

int visible_rows(editor_t *editor) {
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &editor->ws);
    return editor->ws.ws_row - 1;
}
