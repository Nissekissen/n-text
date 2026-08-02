#include "rope.h"
#include <string.h>
#include "renderer.h"

void rope_init(RopeNode* root) {
    root->left = NULL;
    root->right = NULL;
    root->weight = 0;
    root->str = malloc(LEAF_MAX_SIZE);
}

void rope_collect(RopeNode* root, char** buf, size_t* buf_len, size_t* buf_cap) {
    Stack stack;
    stack_init(&stack);

    RopeNode *node = root;
    while (node != NULL) {
        stack_push(&stack, *node);
        node = node->left;
    }

    node = &(stack.data[(stack.size - 1)]);

    safe_append(buf, buf_len, buf_cap, node->str, node->weight);

    // We are at left-most leaf node.
    // Go up one level (if we are not at root) and go once to the right, then repeat
    rope_collect_iter(root, &stack, buf, buf_len, buf_cap);
    
    free(stack.data);
}

void rope_collect_iter(RopeNode* root, Stack* stack, char** buf, size_t* buf_len, size_t* buf_cap) {
    if (stack->size == 0) { return; }

    RopeNode* result = root;
    RopeNode current = stack_pop(stack);

    RopeNode* right = current.right;
    if (right != NULL) {
        stack_push(stack, *right);
    
        RopeNode *node = right->left;
        while (node != NULL) {
            stack_push(stack, *node);
            node = node->left;
        }
        node = &stack->data[stack->size - 1];

        safe_append(buf, buf_len, buf_cap, node->str, node->weight);
        result = node;

    }
        
    rope_collect_iter(root, stack, buf, buf_len, buf_cap);
}

void rope_insert(RopeNode* root, int idx, const char* str) {
    size_t str_len = strlen(str);
    // Insert a string at the start of idx
    if (root->str != NULL) {
        // Base case, do the insertion
        if (root->weight + str_len >= LEAF_MAX_SIZE) {
            // Split it
        } else {
            safe_insert(&root->str, &root->weight, str, str_len, idx);
        }

        return;

    }

    if (idx >= root->weight) {
        return rope_insert(root->right, idx - root->weight, str);
    }

    rope_insert(root->left, idx, str);
    root->weight += str_len;

}

RopeNode* rope_index(RopeNode* node, int startIndex) {
    if (node->str != NULL) {
        return node;
    }

    if (startIndex >= node->weight) {
        return rope_index(node->right, startIndex - node->weight);
    }

    return rope_index(node->left, startIndex);
}

void safe_append(char** buf, size_t* buf_len, size_t* buf_cap, const char* str, size_t str_len) {
    if (*buf_len + str_len > *buf_cap) {
        size_t new_cap = *buf_len + str_len;
        char* tmp = realloc(*buf, new_cap);
        if (!tmp) { die("realloc error"); }
        *buf = tmp;
        *buf_cap = new_cap;
    }

    memcpy(*buf + *buf_len, str, str_len);
    *buf_len += str_len;
}

void safe_insert(char** buf, uint16_t* buf_len, const char* str, size_t str_len, int offset) {
    // move the tail over by str_len
    // insert str at the old index
    
    memmove(*buf + offset + str_len, *buf + offset, *buf_len - offset);
    memcpy(*buf + offset, str, str_len);

    *buf_len += str_len;
}

void stack_init(Stack* s) {
    s->capacity = 8;
    s->size = 0;
    s->data = malloc(s->capacity * sizeof(RopeNode));
}

void stack_push(Stack* s, RopeNode data) {
    if (s->size == s->capacity) {
        s->capacity *= 2;
        s->data = realloc(s->data, s->capacity * sizeof(RopeNode));
    }
    s->data[s->size++] = data;
}

RopeNode stack_pop(Stack *s) {
    return s->data[--s->size];
}
