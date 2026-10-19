#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "renderer.h"
#include "rope.h"
#include "cursor.h"
#include "editor.h"

void print_cursor_debug(editor_t *editor) {
    struct winsize ws;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws); // ws.ws_row / ws.ws_col = terminal size
    size_t total_newlines = rope_total_newlines(&editor->root);
    char seq[64];
    int len = snprintf(seq, sizeof(seq),
        "\x1b""7"                 // DECSC: save cursor pos + attrs
        "\x1b[%d;%dH"              // move to bottom-right-ish corner
        "(%d,%d) %d, %d"                  // the debug text
        "\x1b""8",                 // DECRC: restore cursor pos + attrs
        editor->ws.ws_row, editor->ws.ws_col - 40, (int)editor->cursor.row, (int)editor->cursor.column, (int)total_newlines, (int)editor->cursor.offset);

    write(STDOUT_FILENO, seq, len);
}

void print_key_buf_debug(editor_t *editor, input_event_t event) {
    char seq[128];
    int len = snprintf(seq, sizeof(seq),
            "\x1b""7"
            "\x1b[%d;%dH"
            "keycode: %d, modifiers: %x"
            "\x1b""8",
            editor->ws.ws_row, editor->ws.ws_col - 40, event.keyCode, event.modifiers);

    write(STDOUT_FILENO, seq, len);
}

int main(int argc, char** argv) {
    if (argc >= 2) editor_set_filename(argv[1]);

    editor_init();

    editor_render();

    while (1) {
        input_event_t event = read_key();
        editor_handle_event(event);
        editor_render();
    }

    return 0;
}
