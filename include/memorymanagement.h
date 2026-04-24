// include/memorymanagement.h
#ifndef __MEMORYMANAGEMENT_H
#define __MEMORYMANAGEMENT_H
#include "common/types.h"

struct MemoryChunk {
    MemoryChunk *next, *prev;
    bool allocated;
    size_t size;
};

class MemoryManager {
protected:
    MemoryChunk* first;
public:
    static MemoryManager *ActiveMemoryManager;
    MemoryManager(size_t start, size_t size);
    ~MemoryManager();
    void* malloc(size_t size);
    void  free(void* ptr);
};

void* operator new(size_t size);
void* operator new[](size_t size);
void* operator new(size_t size, void* ptr);
void* operator new[](size_t size, void* ptr);
void  operator delete(void* ptr);
void  operator delete[](void* ptr);

#endif