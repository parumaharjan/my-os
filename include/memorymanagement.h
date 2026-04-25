#ifndef __MYOS__MEMORYMANAGEMENT_H
#define __MYOS__MEMORYMANAGEMENT_H

#include <common/types.h>

struct MemoryChunk
{
    MemoryChunk*            next;
    MemoryChunk*            prev;
    bool                    allocated;
    myos::common::uint32_t  size;
};

class MemoryManager
{
public:
    MemoryChunk*         first;
    static MemoryManager* active;

    MemoryManager(myos::common::uint32_t start, myos::common::uint32_t size);

    void* malloc(myos::common::uint32_t size);
    void  free(void* ptr);
};

void* operator new(myos::common::uint32_t size);
void* operator new[](myos::common::uint32_t size);
void  operator delete(void* ptr);
void  operator delete[](void* ptr);

#endif