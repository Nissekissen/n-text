#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define LEAF_MAX_SIZE 1024
#define TAB_WIDTH 4
#define LEFT_MARGIN 5

typedef struct rope_node_t {
    struct rope_node_t* left;
    struct rope_node_t* right;
    uint16_t weight;
    size_t newlines;
    char* str;
} rope_node_t;

typedef struct {
    rope_node_t* data;
    size_t size;
    size_t capacity;
} rope_stack_t;

typedef struct {
    size_t line;
    size_t segment;
} line_segment_t;

static inline size_t max_size(size_t a, size_t b) { return a > b ? a : b; }
static inline size_t min_size(size_t a, size_t b) { return a < b ? a : b; }

void rope_init(rope_node_t* root);
void rope_collect(rope_node_t* root, char** buf, size_t* buf_len, size_t* buf_cap);
void rope_collect_iter(rope_node_t* root, rope_stack_t* stack, char** buf, size_t* buf_len, size_t* buf_cap);
void rope_collect_between(rope_node_t* node, size_t from, size_t to, char** buf, size_t* buf_len, size_t* buf_cap);
size_t rope_count_chars_between(rope_node_t* node, size_t from, size_t to);
void rope_insert(rope_node_t* root, int idx, const char* str, size_t str_len);
void rope_delete(rope_node_t* root, int idx, size_t len);
rope_node_t* rope_index(rope_node_t* node, int startIndex);
void rope_split_node(rope_node_t* node);

size_t rope_line_of_offset(rope_node_t *node, size_t offset);
size_t offset_of_nth_newline(rope_node_t *node, size_t n);
size_t rope_offset_of_line_start(rope_node_t *root, size_t line);
size_t rope_line_length(rope_node_t *root, size_t line, size_t total_lines);
size_t rope_total_newlines(rope_node_t *node);
size_t rope_total_length(rope_node_t *node);
size_t rope_segment_count(rope_node_t *root, size_t line, size_t visible_width, size_t total_lines);
size_t rope_segment_count_between(rope_node_t *root, size_t from, size_t to, size_t visible_width);
line_segment_t rope_line_of_visual_row(rope_node_t *root, size_t from_line, size_t target_row, size_t visible_width, size_t total_lines);

void safe_append(char **buf, size_t* buf_len, size_t* buf_cap, const char* str, size_t str_len);
void safe_insert(char **buf, uint16_t* buf_len, const char* str, size_t str_len, int offset);

void stack_init(rope_stack_t* s);
void stack_push(rope_stack_t* s, rope_node_t data);
rope_node_t stack_pop(rope_stack_t *s);

size_t count_newlines(const char* str, size_t str_len);
