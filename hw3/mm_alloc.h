/*
 * mm_alloc.h
 *
 * A clone of the interface documented in "man 3 malloc".
 */

#pragma once

#include <stdlib.h>
#include <stdbool.h>

void *mm_malloc(size_t size);
void *mm_realloc(void *ptr, size_t size);
void mm_free(void *ptr);
struct metadata* metadata_init(size_t new_size, struct metadata* prev);

struct metadata { //17 bytes
    size_t size; // 4 bytes
    bool free; // 1 byte
    struct metadata* prev; //4 bytes
    struct metadata* next; //4 bytes
    void* current_block; //4 bytes
};