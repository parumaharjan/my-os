// src/memorymanagement.cpp
#include "memorymanagement.h"

MemoryManager* MemoryManager::ActiveMemoryManager = 0;

MemoryManager::MemoryManager(size_t start, size_t size) {
    ActiveMemoryManager = this;
    if (size < sizeof(MemoryChunk)) { first = 0; return; }
    first = (MemoryChunk*)start;
    first->allocated = false;
    first->prev = 0;
    first->next = 0;
    first->size = size - sizeof(MemoryChunk);
}

void* MemoryManager::malloc(size_t size) {
    MemoryChunk* result = 0;
    for (MemoryChunk* chunk = first; chunk != 0; chunk = chunk->next) {
        if (!chunk->allocated && chunk->size >= size) {
            result = chunk;
            break;
        }
    }
    if (result == 0) return 0;

    // Split chunk if large enough
    if (result->size >= size + sizeof(MemoryChunk) + 1) {
        MemoryChunk* tmp = (MemoryChunk*)((size_t)result + sizeof(MemoryChunk) + size);
        tmp->allocated = false;
        tmp->size = result->size - size - sizeof(MemoryChunk);
        tmp->prev = result;
        tmp->next = result->next;
        if (tmp->next) tmp->next->prev = tmp;
        result->next = tmp;
        result->size = size;
    }
    result->allocated = true;
    return (void*)((size_t)result + sizeof(MemoryChunk));
}

void MemoryManager::free(void* ptr) {
    MemoryChunk* chunk = (MemoryChunk*)((size_t)ptr - sizeof(MemoryChunk));
    chunk->allocated = false;

    // Merge with previous
    if (chunk->prev && !chunk->prev->allocated) {
        chunk->prev->next = chunk->next;
        chunk->prev->size += chunk->size + sizeof(MemoryChunk);
        if (chunk->next) chunk->next->prev = chunk->prev;
        chunk = chunk->prev;
    }
    // Merge with next
    if (chunk->next && !chunk->next->allocated) {
        chunk->size += chunk->next->size + sizeof(MemoryChunk);
        chunk->next = chunk->next->next;
        if (chunk->next) chunk->next->prev = chunk;
    }
}

void* operator new(size_t size) { return MemoryManager::ActiveMemoryManager->malloc(size); }
void* operator new[](size_t size) { return MemoryManager::ActiveMemoryManager->malloc(size); }
void operator delete(void* ptr) { return MemoryManager::ActiveMemoryManager->free(ptr); }
void operator delete[](void* ptr) { return MemoryManager::ActiveMemoryManager->free(ptr); }