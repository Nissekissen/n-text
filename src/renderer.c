#include "renderer.h"
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>


struct termios orig_termos;

static char encoding_table[] = {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H',
                                'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P',
                                'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X',
                                'Y', 'Z', 'a', 'b', 'c', 'd', 'e', 'f',
                                'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
                                'o', 'p', 'q', 'r', 's', 't', 'u', 'v',
                                'w', 'x', 'y', 'z', '0', '1', '2', '3',
                                '4', '5', '6', '7', '8', '9', '+', '/'};
static int mod_table[] = {0, 2, 1};

void Renderer_Init(void) {

    write(STDOUT_FILENO, "\x1b[?1049h", 8); // Enter alt screen
    enable_raw_mode();

    write(STDOUT_FILENO, "\x1b[?1000h", 8); // enable mouse tracking
    write(STDOUT_FILENO, "\x1b[?1006h", 8); // SGR extended mode
    write(STDOUT_FILENO, "\x1b[?1002h", 8); // Enable mouse dragging
    write(STDOUT_FILENO, "\x1b[?2004h", 8); // enable bracketed paste mode
    write(STDOUT_FILENO, "\x1b[6 q", 5); // Set cursor shape
}

void Renderer_Exit(void) {

    write(STDOUT_FILENO, "\x1b[2J", 4); // clear screen
    write(STDOUT_FILENO, "\x1b[H", 3);  // move cursor to home
    
    write(STDOUT_FILENO, "\x1b[1 q", 5); // reset cursor
    
    write(STDOUT_FILENO, "\x1b[?1049l", 8); // back to original terminal

    write(STDOUT_FILENO, "\x1b[?1000l", 8); // disable mouse tracking
    write(STDOUT_FILENO, "\x1b[?1006l", 8); // disable SGR extended mode
    write(STDOUT_FILENO, "\x1b[?1002l", 8); // disbale mouse dragging
    write(STDOUT_FILENO, "\x1b[?2004l", 8); // disable bracketed paste mode
    exit(0);
}

void Renderer_print_line_numbers(size_t total_lines, size_t start_line) {
    
    // Set color to gray
    write(STDOUT_FILENO, "\033[38;5;242m", 11);
    for (size_t i = 0; i <= total_lines; i++) {
        // Move to start of line
        Renderer_move_to(i + 1, 0);
        
        char buf[8];
        int len = snprintf(buf, sizeof(buf), "%4zu", i + start_line + 1);
        write(STDOUT_FILENO, buf, len);
        
    }

    // Reset color
    write(STDOUT_FILENO, "\033[0m", 4);
}

void Renderer_print_line_number(size_t number) {
    
    write(STDOUT_FILENO, "\033[38;5;242m", 11); // Set color to gray
    
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "%4zu ", number);
    write(STDOUT_FILENO, buf, len);

    write(STDOUT_FILENO, "\033[0m", 4); // Reset color
}

void Renderer_move_to(int row, int col) {
    char buf[12];
    int len = snprintf(buf, 12, "\x1b[%d;%dH", row, col);
    write(STDOUT_FILENO, buf, len);
}

void Renderer_Print(int c) {
    if (c == '\n') {
        write(STDOUT_FILENO, "\r\n", 2);

        // Move LEFT_MARGIN characters to the right
        Renderer_move_right(LEFT_MARGIN);
        return;
    }
    write(STDOUT_FILENO, &c, 1);
}

void Renderer_print_buf(char* buf, size_t buf_len) {
    for (int i = 0; i < buf_len; i++) {
        Renderer_Print(buf[i]);
    }
}

void Renderer_print_cursor(Cursor *cursor, RopeNode *root, size_t line_offset, size_t visible_rows, size_t visible_width) {
    size_t cursor_visual_row = rope_segment_count_between(root, line_offset, cursor->row, visible_width) + cursor_segment(cursor, visible_width);

    if (cursor->row < line_offset || cursor_visual_row >= visible_rows) {
        write(STDOUT_FILENO, "\x1b[?25l", 6); // hide cursor, it's scrolled out of view
        return;
    }
    write(STDOUT_FILENO, "\x1b[?25h", 6); // ensure visible
    Renderer_move_to((int) (cursor_visual_row) + 1 + 1, (int) (cursor->column % visible_width) + 1 + LEFT_MARGIN);
}

void Renderer_move_right(size_t chars) {
    char buf[8];
    int len = snprintf(buf, sizeof(buf), "\x1b[%zuC", chars);
    write(STDOUT_FILENO, buf, len);
}

void Renderer_set_clipboard(const char *data, size_t len) {   
#ifdef __APPLE__
    FILE *pipe = popen("pbcopy", "w");
    if (pipe) {
        fwrite(data, 1, len, pipe);
        pclose(pipe);
    }
#else
    size_t base64_len = 0;
    char *base64_data = base64_encode((const unsigned char*)data, len, &base64_len);
    
    char *buf = malloc(base64_len + 20);
    size_t buf_len = snprintf(
            buf,
            base64_len + 20,
            "\033]52;c;%s\a",
            base64_data);
    
    write(STDOUT_FILENO, buf, buf_len);

    free(buf);
    free(base64_data);
#endif
}

char *Renderer_get_clipboard(void) {
#ifdef __APPLE__
    FILE *pipe = popen("pbpaste", "r");
    if (!pipe) return NULL;

    char *buf = NULL;
    size_t buf_len = 0, buf_cap = 0;

    char chunk[4096];
    size_t n;

    while ((n = fread(chunk, 1, sizeof(chunk), pipe)) > 0) {
        safe_append(&buf, &buf_len, &buf_cap, chunk, n);
    }

    int status = pclose(pipe);

    if (status != 0) {
        free(buf);
        return NULL;
    }

    safe_append(&buf, &buf_len, &buf_cap, "", 1);

    return buf;
#else
    return NULL;
#endif
}

void die(const char *s) {
    perror(s);
    Renderer_Exit();
}

void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termos);
}

void clear_screen(void) {
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termos);
    atexit(disable_raw_mode);

    struct termios raw = orig_termos;
    cfmakeraw(&raw);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

void normalize_line_endings(InputEvent *event) {
    size_t w = 0;
    for (size_t r = 0; r < event->paste_len; r++) {
        if (r < event->paste_len - 1 && event->paste_data[r] == '\r' && event->paste_data[r+ 1] == '\n') continue;
        if (event->paste_data[r] == '\r') {
            event->paste_data[w] = '\n';
            w++;
            continue;
        }

        event->paste_data[w] = event->paste_data[r];
        w++;
    }

    event->paste_len = w;
}

void scroll_to_cursor(Cursor *cursor, RopeNode *root, size_t *line_offset, size_t visible_rows, size_t visible_width) {
    size_t cursor_visual_row = rope_segment_count_between(root, *line_offset, cursor->row, visible_width) + cursor_segment(cursor, visible_width);
    if (cursor->row < *line_offset) *line_offset = cursor->row; // Scroll up

    if (cursor_visual_row >= visible_rows) {
        // *line_offset = cursor->row - ws->ws_row + 1; // Scroll down
        
        while (1) {
            cursor_visual_row = rope_segment_count_between(root, *line_offset, cursor->row, visible_width) + cursor_segment(cursor, visible_width);
            if (cursor_visual_row < visible_rows) break;
            (*line_offset)++;
        }
    }
}

int read_ascii_number(unsigned char *c) {
    int value = 0;
    while (read(STDIN_FILENO, c, 1) == 1 && *c >= '0' && *c <= '9') {
        value = value * 10 + (*c - '0');
    }

    return value;
}

int is_csi_arrow_key(unsigned char c) {
    return c == 'A' || c == 'B' || c == 'C' || c == 'D';
}

int csi_get_arrow_key(unsigned char c) {
    switch (c) {
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
        default: return 0;
    }
}

InputEvent read_key(void) {
    InputEvent event = {0};
    
    unsigned char c;
    ssize_t nread;
    while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
        if (nread == -1 && errno != EAGAIN) die("read");
    }
    
    if (c == '\r') { event.bytes[0] = '\n'; event.byte_count = 1; return event; };
    
    if (c == CTRL_KEY('s')) { event.keyCode =          SAVE; return event; }
    if (c == CTRL_KEY('S')) { event.keyCode =       SAVE_AS; return event; }
    if (c == CTRL_KEY('q')) { event.keyCode =          QUIT; return event; }
    if (c == CTRL_KEY(' ')) { event.keyCode = TOGGLE_SELECT; return event; }
    if (c == CTRL_KEY('x')) { event.keyCode =           CUT; return event; }
    if (c == CTRL_KEY('c')) { event.keyCode =          COPY; return event; }
    if (c == CTRL_KEY('v')) { event.keyCode =         PASTE; return event; }
    if (c == 0x7F)          { event.keyCode =     BACKSPACE; return event; }

    if (c == '\t') {
        for (int i = 0; i < TAB_WIDTH; i++) event.bytes[i] = ' ';
        event.byte_count = TAB_WIDTH;
        return event;
    }

    if (c == '\x1b') {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);

        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 50000; // 50 ms
        
        int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        if (ready == 0) { event.keyCode = ESC; return event; }

        char bracketHopefully;
        if (read(STDIN_FILENO, &bracketHopefully, 1) != 1) { event.keyCode = ESC; return event; }
        if (bracketHopefully != '[') { // ALT + key
            event.modifiers |= MOD_ALT;
            event.bytes[0] = bracketHopefully;
            event.byte_count = 1;
            return event;
        }
        
        CSI_Parser_return parse_value = csi_parse();
        if (parse_value.privateMarker == '<') {
            // mouse scrolling / click
            if (parse_value.parameterCount < 3) { event.keyCode = ESC; return event; } // Malformed

            int Cb = parse_value.parameters[0];
            int Cx = parse_value.parameters[1];
            int Cy = parse_value.parameters[2];

            if (Cb == 64) { event.keyCode = MOUSE_SCROLL_UP;   return event; }
            if (Cb == 65) { event.keyCode = MOUSE_SCROLL_DOWN; return event; }

            event.click_row = Cy;
            event.click_col = Cx;

            if (Cb == 32 && parse_value.finalChar == 'M') { event.keyCode =    MOUSE_DRAG; return event; }
            if (Cb ==  0 && parse_value.finalChar == 'm') { event.keyCode = MOUSE_RELEASE; return event; }
            if (Cb ==  0 && parse_value.finalChar == 'M') { event.keyCode =   MOUSE_CLICK; return event; }
        }
        
        if (is_csi_arrow_key(parse_value.finalChar)) {
            if (parse_value.parameterCount == 0) {
                event.keyCode = csi_get_arrow_key(parse_value.finalChar);
                return event;
            }

            if (parse_value.parameterCount >= 2) event.modifiers = parse_value.parameters[1] - 1;

            event.keyCode = csi_get_arrow_key(parse_value.finalChar);
            return event;
        }

        if (parse_value.finalChar == '~' && parse_value.parameterCount >= 1) {
            if (parse_value.parameters[0] == 3) { event.keyCode = FORWARD_DELETE; return event; }
            
            if (parse_value.parameters[0] == 200) {
                // paste start
                // Read from stdin until we see ESC[201~
                event.paste_data = malloc(1024);
                size_t paste_cap = 1024;

                while ((nread = read(STDIN_FILENO, &c, 1)) == 1) {
                    safe_append(&event.paste_data, &event.paste_len, &paste_cap, (const char*)&c, 1);

                    if (event.paste_len >= 6 && memcmp(event.paste_data + event.paste_len - 6, "\x1b[201~", 6) == 0) {
                        event.paste_len -= 6;
                        break;
                    }
                }
                if (nread == -1 && errno != EAGAIN) die("read");

                normalize_line_endings(&event);

                return event;
            }
        }

        if (parse_value.parameterCount >= 1 && parse_value.parameters[0] == 3 && parse_value.finalChar == '~') { // Forward delete key
            event.keyCode = FORWARD_DELETE;
            return event;
        }

        event.keyCode = ESC;
        return event;
    }
    
    // Regular characters
    int len = utf8_seq_len(c);
    event.bytes[0] = c;
    nread = read(STDIN_FILENO, event.bytes + 1, len - 1);
    if (nread != len - 1) die("read");
    event.byte_count = len;
    return event;
}
int is_csi_final_byte(unsigned char c) {
    return c >= 0x40 && c <= 0x7E;
}

int is_csi_private_marker(unsigned char c) {
    return c == '<' || c == '>' || c == '=' || c == '?';
}

int is_csi_parameter_byte(unsigned char c) {
    return c >= 0x30 && c <= 0x3F;
}

CSI_Parser_return csi_parse() {
    // ESC[ has already been read.
    CSI_Parser_return return_val = {0};
    char c;
    int paramFlag = 0;
    while (read(STDIN_FILENO, &c, 1) == 1) {
        if (is_csi_private_marker(c)) { return_val.privateMarker = c; continue; }
        if (is_csi_final_byte(c)) {
            if (paramFlag) return_val.parameterCount++;
            return_val.finalChar = c;
            break;
        }
        
        if (!is_csi_parameter_byte(c)) continue; // Weird byte I guess?
        
        paramFlag = 1;

        // Parse as integer
        if (c == ';') { return_val.parameterCount++; continue; }
        if (return_val.parameterCount < 4) return_val.parameters[return_val.parameterCount] = return_val.parameters[return_val.parameterCount] * 10 + (c - '0');
    }

    return return_val;
}

int utf8_seq_len(unsigned char lead) {
    if ((lead & 0x80) == 0x00) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 1;
}

char* base64_encode(const unsigned char *data, size_t input_length, size_t *output_length) {
    *output_length = 4 * ((input_length + 2) / 3);

    char *encoded_data = malloc(*output_length + 1);
    if (encoded_data == NULL) return NULL;
    
    for (int i = 0, j = 0; i < input_length;) {

        uint32_t octet_a = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_b = i < input_length ? (unsigned char)data[i++] : 0;
        uint32_t octet_c = i < input_length ? (unsigned char)data[i++] : 0;

        uint32_t triple = (octet_a << 0x10) + (octet_b << 0x08) + octet_c;

        encoded_data[j++] = encoding_table[(triple >> 3 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 2 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 1 * 6) & 0x3F];
        encoded_data[j++] = encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    for (int i = 0; i < mod_table[input_length % 3]; i++)
        encoded_data[*output_length - 1 - i] = '=';

    encoded_data[*output_length] = '\0';

    return encoded_data;
}
