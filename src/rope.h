#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define LEAF_MAX_SIZE 1024

typedef struct RopeNode {
    struct RopeNode* left;
    struct RopeNode* right;
    uint16_t weight;
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
void rope_insert(RopeNode* root, int idx, const char* str);
RopeNode* rope_index(RopeNode* node, int startIndex);
void rope_split_node(RopeNode* node);

void safe_append(char **buf, size_t* buf_len, size_t* buf_cap, const char* str, size_t str_len);
void safe_insert(char **buf, uint16_t* buf_len, const char* str, size_t str_len, int offset);

void stack_init(Stack* s);
void stack_push(Stack* s, RopeNode data);
RopeNode stack_pop(Stack *s);
