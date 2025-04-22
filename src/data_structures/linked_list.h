#pragma once

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct ListNode {
    void* data;
    struct ListNode* next;
} ListNode;

typedef struct {
    ListNode* head;
    ListNode* tail;
    size_t dataSize;
    int size;
} linked_list;

void initList(linked_list* list, size_t dataSize);
void append(linked_list* list, void* item);
void* removeFront(linked_list* list);
void* getFront(linked_list* list);
int isListEmpty(linked_list* list);
void clearList(linked_list* list);

// Endpoints for deque.h
void prepend(linked_list* list, void* item);
void* removeBack(linked_list* list);
void* getBack(linked_list* list);

