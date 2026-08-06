#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define LEAF_MAX_SIZE 1024

typedef struct RopeNode {
    struct RopeNode* left;
    struct RopeNode* right;
    uint16_t weight;
    size_t newlines;
    char* str;
} RopeNode;

typedef struct {
    RopeNode* data;
    size_t size;
    size_t capacity;
} Stack;

void rope_init(RopeNode* root);
void rope_collect(RopeNode* root, char** buf, size_t* buf_len, size_t* buf_cap);
void rope_collect_iter(RopeNode* root, Stack* stack, char** buf, size_t* buf_len, size_t* buf_cap);
void rope_collect_between(RopeNode* node, size_t from, size_t to, char** buf, size_t* buf_len, size_t* buf_cap);
size_t rope_count_chars_between(RopeNode* node, size_t from, size_t to);
void rope_insert(RopeNode* root, int idx, const char* str, size_t str_len);
RopeNode* rope_index(RopeNode* node, int startIndex);
void rope_split_node(RopeNode* node);

size_t rope_line_of_offset(RopeNode *node, size_t offset);
size_t offset_of_nth_newline(RopeNode *node, size_t n);
size_t rope_offset_of_line_start(RopeNode *root, size_t line);
size_t rope_line_length(RopeNode *root, size_t line, size_t total_lines);
size_t rope_total_newlines(RopeNode *node);

void safe_append(char **buf, size_t* buf_len, size_t* buf_cap, const char* str, size_t str_len);
void safe_insert(char **buf, uint16_t* buf_len, const char* str, size_t str_len, int offset);

void stack_init(Stack* s);
void stack_push(Stack* s, RopeNode data);
RopeNode stack_pop(Stack *s);

size_t count_newlines(const char* str, size_t str_len);
