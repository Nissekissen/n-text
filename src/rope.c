#include "rope.h"
#include <string.h>
#include "renderer.h"

void rope_init(RopeNode* root) {
    root->left = NULL;
    root->right = NULL;
    root->weight = 0;
    root->newlines = 0;
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

void rope_collect_between(RopeNode* node, size_t from, size_t to, char** buf, size_t* buf_len, size_t* buf_cap) {
    if (node == NULL || from >= to) return;

    if (node->str != NULL) {
        // Leaf: clamp [from, to) to this leaf's own [0, weight) range
        size_t start = from < node->weight ? from : node->weight;
        size_t end = to < node->weight ? to : node->weight;
        if (end > start) {
            safe_append(buf, buf_len, buf_cap, node->str + start, end - start);
        }
        return;
    }

    // Internal node: left subtree covers [0, weight), right covers [weight, ...)
    if (from < node->weight) {
        rope_collect_between(node->left, from, to, buf, buf_len, buf_cap);
    }
    if (to > node->weight) {
        size_t right_from = from > node->weight ? from - node->weight : 0;
        rope_collect_between(node->right, right_from, to - node->weight, buf, buf_len, buf_cap);
    }
}

size_t rope_count_chars_between(RopeNode* node, size_t from, size_t to) {
    if (node == NULL || from >= to) return 0;

    if (node->str != NULL) {
        size_t start = from < node->weight ? from : node->weight;
        size_t end = to < node->weight ? to : node->weight;
        size_t count = 0;
        for (size_t i = start; i < end; i++) {
            if (((unsigned char)node->str[i] & 0xC0) != 0x80) count++;
        }
        return count;
    }

    size_t count = 0;
    if (from < node->weight) {
        count += rope_count_chars_between(node->left, from, to);
    }
    if (to > node->weight) {
        size_t right_from = from > node->weight ? from - node->weight : 0;
        count += rope_count_chars_between(node->right, right_from, to - node->weight);
    }
    return count;
}

void rope_insert(RopeNode* root, int idx, const char* str, size_t str_len) {
    // Insert a string at the start of idx
    if (root->str != NULL) {
        // Base case, do the insertion
        if (root->weight + str_len >= LEAF_MAX_SIZE) {
            rope_split_node(root);
            return rope_insert(root, idx, str, str_len);
        }

        safe_insert(&root->str, &root->weight, str, str_len, idx);
        root->newlines += count_newlines(str, str_len);
        return;

    }

    if (idx >= root->weight) {
        return rope_insert(root->right, idx - root->weight, str, str_len);
    }

    rope_insert(root->left, idx, str, str_len);
    root->weight += str_len;
    root->newlines += count_newlines(str, str_len);
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

void rope_split_node(RopeNode* node) {
    // Create new nodes left and right and link them to the node. Split the string in half and put each half in the new leaf nodes. Return if not a leaf node
    if (node->str == NULL) { return; }

    RopeNode *left = malloc(sizeof(RopeNode));
    RopeNode *right = malloc(sizeof(RopeNode));

    rope_init(left);
    rope_init(right);

    node->left = left;
    node->right = right;

    size_t left_len = node->weight / 2;
    size_t right_len = node->weight - left_len;

    // Split node->str in half and put it in left->str and right->str
    memcpy(left->str, node->str, left_len);
    memcpy(right->str, &node->str[left_len], right_len);

    free(node->str);
    node->str = NULL;

    // update weights
    left->weight = left_len;
    right->weight = right_len;
    node->weight = left->weight;

    left->newlines  = count_newlines(left->str, left->weight);
    right->newlines = count_newlines(right->str, right->weight);
    node->newlines = left->newlines;
}

size_t rope_line_of_offset(RopeNode *node, size_t offset) {
    size_t line = 0;
    while (node->str == NULL) {
        if (offset < node->left->weight) {
            node = node->left;
            continue;
        }

        line += node->left->newlines;
        offset -= node->left->weight;
        node = node->right;
    }

    // leaf node
    for (size_t i = 0; i < offset; i++)
        if (node->str[i] == '\n') line++;
    
    return line;
}

size_t offset_of_nth_newline(RopeNode *node, size_t n) {
    size_t offset = 0;
    while (node->str == NULL) {
        if (n < node->left->newlines) {
            node = node->left;
            continue;
        }

        n -= node->left->newlines;
        offset += node->left->weight;
        node = node->right;
    }

    // Leaf node
    size_t count = 0;
    for (size_t i = 0; i < strlen(node->str); i++) {
        if (node->str[i] == '\n') {
            if (count == n) return offset + i;
            count++;
        }
    }

    // should be unreachable
    return 0;
}

size_t rope_offset_of_line_start(RopeNode *root, size_t line) {
    if (line == 0) return 0;
    return offset_of_nth_newline(root, line - 1) + 1;
}

size_t rope_line_length(RopeNode *root, size_t line, size_t total_lines) {
    size_t start = rope_offset_of_line_start(root, line);
    if (line + 1 >= total_lines) return rope_count_chars_between(root, start, root->weight); // Last line, no trailing newline

    size_t next_start = rope_offset_of_line_start(root, line + 1);
    return rope_count_chars_between(root, start, next_start - 1);
}

size_t rope_total_newlines(RopeNode *node) {
    if (node->str != NULL) return node->newlines;

    return node->newlines + rope_total_newlines(node->right);
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

size_t count_newlines(const char *str, size_t str_len) {
    size_t count = 0;
    for (int i = 0; i < str_len; i++) {
        if (str[i] == '\n') count++;
    }

    return count;
}
