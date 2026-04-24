// src/paging.cpp
#include "paging.h"

PageFrameAllocator::PageFrameAllocator(void* bitmapAddr, size_t memBytes) {
    bitmap = (uint8_t*)bitmapAddr;
    totalPages = memBytes / 4096;
    firstFreePage = 0;
    // Clear bitmap
    for (size_t i = 0; i < (totalPages + 7) / 8; i++)
        bitmap[i] = 0;
}

uint32_t PageFrameAllocator::AllocPage() {
    for (size_t i = firstFreePage; i < totalPages; i++) {
        if (!(bitmap[i/8] & (1 << (i%8)))) {
            bitmap[i/8] |= (1 << (i%8));
            firstFreePage = i + 1;
            return (uint32_t)(i * 4096);
        }
    }
    return 0; // Out of memory
}

void PageFrameAllocator::FreePage(uint32_t physAddr) {
    size_t i = physAddr / 4096;
    bitmap[i/8] &= ~(1 << (i%8));
    if (i < firstFreePage) firstFreePage = i;
}

PageDirectory::PageDirectory(PageFrameAllocator* pfa) {
    entries = (uint32_t*)pfa->AllocPage();
    for (int i = 0; i < 1024; i++)
        entries[i] = 0x00000002; // Not present, RW
}

void PageDirectory::MapPage(uint32_t virt, uint32_t phys, bool rw, bool user) {
    uint32_t pdi = virt >> 22;
    uint32_t pti = (virt >> 12) & 0x3FF;
    uint32_t* table;
    if (!(entries[pdi] & 1)) {
        // Allocate a page table (HACK: for now, use phys addr of table inline)
        // In real use, call pfa->AllocPage() and store here
        table = (uint32_t*)((entries[pdi] & ~0xFFF));
    } else {
        table = (uint32_t*)(entries[pdi] & ~0xFFF);
    }
    table[pti] = (phys & ~0xFFF) | 0x01 | (rw ? 0x02 : 0) | (user ? 0x04 : 0);
    entries[pdi] = (uint32_t)table | 0x01 | (rw ? 0x02 : 0) | (user ? 0x04 : 0);
}

void PageDirectory::Load() {
    asm volatile("mov %0, %%cr3" :: "r"(entries));
}

void PageDirectory::Enable() {
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}