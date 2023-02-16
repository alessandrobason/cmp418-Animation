#include "arena.h"

#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#include <system/allocator.h>
#include <maths/math_utils.h> // gef::max

ArenaNode *ArenaNode::make(IAllocator *allocator, size_t buffer_size) {
    PushAllocInfo("ArenaNode");
    ArenaNode *node = allocator->allocDebug<ArenaNode>(1, "Arena Node");
    assert(node);
    node->data = allocator->allocDebug<uint8_t>(buffer_size, "Arena Node Data");
    node->size = buffer_size;
    PopAllocInfo();
    return node;
}

void ArenaNode::release(IAllocator *allocator, ArenaNode *node) {
    if (!node) {
        return;
    }
    allocator->dealloc(node->data);
    allocator->dealloc(node);
}

bool ArenaNode::hasSpace(size_t needed_size) {
    assert(position <= size);
    return (size - position) >= needed_size;
}

void *ArenaNode::get(size_t object_size) {
    assert(hasSpace(object_size));
    void *buf = static_cast<void*>(data + position);
    position += object_size;
    return buf;
}

void *Arena::alloc(size_t size) {
    checkAllocator();
    // if we don't have an head, make one real quick
    if (!head) {
        size_t new_size = gef::max(arena_min_size, size * 2);
        head = ArenaNode::make(allocator, new_size);
    }
    // traverse the list to find a node with enough free memory
    ArenaNode *node = head;
    while (node) {
        if (node->hasSpace(size)) {
            return node->get(size);
        }
        node = node->next;
    }
    // if there wasn't a node with enought memory, make one
    assert(head);
    size_t new_size = gef::max(head->size, size) * 2;
    ArenaNode *new_head = ArenaNode::make(allocator, new_size);
    new_head->next = head;
    head = new_head;
    return head->get(size);
}

void Arena::dealloc(void *ptr) {
    (void)ptr;
    // no-op
}

void Arena::cleanup() {
    ArenaNode *node = head;
    while (node) {
        ArenaNode *next = node->next;
        ArenaNode::release(allocator, node);
        node = next;
    }
    head = nullptr;
    if (owns_allocator && allocator) {
        allocator->cleanup();
        // call destructor
        allocator->~IAllocator();
        free(allocator);
        allocator = nullptr;
    }
}

Arena::~Arena() {
    cleanup();
}

void Arena::setInitialSize(size_t size) {
    assert(!head);
    checkAllocator();
    head = ArenaNode::make(allocator, size);
}

void Arena::setAllocator(IAllocator *alloc, bool should_own) {
    assert(!allocator || allocator && alloc);
    allocator = alloc;
    owns_allocator = should_own;
}

void Arena::checkAllocator() {
    if (!allocator) {
        allocator = (IAllocator *)calloc(1, sizeof(SimpleAlloc));
        // call constructor
        new (allocator) SimpleAlloc();
        allocator->init();
        owns_allocator = true;
    }
}