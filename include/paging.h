// include/paging.h
#ifndef __PAGING_H
#define __PAGING_H
#include "common/types.h"
#include "memorymanagement.h"

class PageFrameAllocator {
    uint8_t* bitmap;
    size_t   totalPages;
    size_t   firstFreePage;
public:
    PageFrameAllocator(void* bitmapAddr, size_t memBytes);
    uint32_t AllocPage();
    void     FreePage(uint32_t physAddr);
};

class PageDirectory {
    uint32_t* entries; // 1024 entries, each pointing to a page table
public:
    PageDirectory(PageFrameAllocator* pfa);
    void MapPage(uint32_t virt, uint32_t phys, bool rw = true, bool user = false);
    void Load();
    static void Enable();
};
#endif