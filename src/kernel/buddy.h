#pragma once

#include <stdio.h>
#include <stdlib.h>

struct buddy;

struct buddy *buddy_new(unsigned num_of_fragments);
int buddy_alloc(struct buddy *self, size_t size);
void buddy_free(struct buddy *self, int offset);
void buddy_dump(struct buddy *self);
void buddy_destroy(struct buddy *self);

// Add a new function to get the buddy system's total memory size
size_t buddy_get_size(struct buddy *self);
