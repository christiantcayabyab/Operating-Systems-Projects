/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <memory.h>
#include <unistd.h>

struct metadata* first_metadata;

struct metadata* metadata_init(size_t new_size, struct metadata* prev) {
    struct metadata *meta;
    meta = (struct metadata *) sbrk(new_size);
    if (meta == (void *) -1) {
        return NULL;
    }
    meta->size = new_size;
    meta->free = false;
    meta->prev = prev;
    meta->next = NULL;
    meta->current_block = meta + sizeof(struct metadata);
    memset(meta->current_block, 0, meta->size);
    if (prev != NULL) {
        prev->next = meta;
    }
    return meta;
}

void *mm_malloc(size_t size) {
    if (size == 0) {
        return NULL;
    }
    size_t new_size = (sizeof(struct metadata) + size);
    if (first_metadata == NULL) {
        first_metadata = metadata_init(new_size, NULL);
        if (first_metadata == NULL) {
            return NULL;
        }
        return first_metadata->current_block;
    }
    struct metadata *meta_head = first_metadata;
    while (true) {
        while (meta_head->next != NULL && meta_head->free == false) {
            meta_head = meta_head->next;
        }
        if (meta_head->free == true) {
            if (meta_head->size >= size) {
                struct metadata *meta;
                meta = meta_head;
                size_t size_diff = meta->size - size;
                meta->free = false;
                if (size_diff > sizeof(struct metadata)) {
                    meta->size = size;
                    struct metadata *split_meta =  (meta->current_block + size);
                    split_meta->next = meta->next;
                    meta->next = split_meta;
                    split_meta->prev = meta;
                    split_meta->free = true;
                    split_meta->size = size_diff - sizeof(struct metadata);
                    split_meta->current_block = split_meta + sizeof(struct metadata);
                    memset(split_meta->current_block, 0, (size_diff - sizeof(struct metadata)));
                }
                memset(meta->current_block, 0, meta->size);
                return meta->current_block;
            } else if (meta_head->next != NULL) {
                meta_head = meta_head->next;
                continue;
            }
        }
        if (meta_head->next == NULL) {
            struct metadata *meta = metadata_init(new_size, meta_head);
            if (meta == NULL) {
                return NULL;
            } else {
                memset(meta->current_block, 0, size);
                return meta->current_block;
            }
        }
    }
    return NULL;
}


void *mm_realloc(void *ptr, size_t size) {
    struct metadata *meta = (struct metadata*) ptr - sizeof(struct metadata);
    size_t len = meta->size;
    mm_free(ptr);
    void *new_place = mm_malloc(size);
    if (len > size) {
        len = size;
    }
    memcpy(new_place, ptr, len);
    return NULL;
}

void mm_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    struct metadata *meta = first_metadata;
    bool found = false;
    if (meta == NULL) {
        return;
    }
    while(!found && (meta != NULL)) {
        if (meta->current_block == ptr) {
            found = true;
        } else {
            if (meta->next != NULL) {
                meta = meta->next;
            } else {
                return;
            }
        }
    }
    if (meta == NULL) {
        return;
    }
    bool prev_free = false;
    bool next_free = false;
    if (meta != NULL) {
        if (meta->prev != NULL) {
            if (meta->prev->free == true) {
                prev_free = true;
            }
        }
        if (meta->next != NULL) {
            if (meta->next->free == true) {
                next_free = true;
            }
        }
    } else {
        return;
    }
    size_t freeing_block_size = meta->size;
    if (prev_free && next_free) {
        size_t top_block_size = meta->next->size;
        size_t bottom_block_size = meta->prev->size;
        meta->prev->next = meta->next->next;
        meta = meta->prev;
        if (meta->next != NULL) {
            meta->next->prev = meta;
        }
        meta->size = freeing_block_size + sizeof(struct metadata) + bottom_block_size + sizeof(struct metadata) + top_block_size;
        meta->free = true;
    } else if (next_free) {
        size_t top_block_size = meta->next->size;
        meta->next = meta->next->next;
        if (meta->next != NULL) {
            meta->next->prev = meta;
        }
        meta->size = freeing_block_size + sizeof(struct metadata) + top_block_size;
        meta->free = true;
    } else if (prev_free) {
        size_t bottom_block_size = meta->prev->size;
        meta->prev->next = meta->next;
        meta = meta->prev;
        if (meta->next != NULL) {
            meta->next->prev = meta;
        }
        meta->size = freeing_block_size + sizeof(struct metadata) + bottom_block_size;
        meta->free = true;
    } else {
        meta->free = true;
    }
}
