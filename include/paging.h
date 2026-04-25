#ifndef __MYOS__PAGING_H
#define __MYOS__PAGING_H

#include <common/types.h>

namespace myos
{
    // ── PageDirectory ────────────────────────────────────────────────────────
    // One page directory = 1024 entries, each covering 4 MB (with 4 MB pages).
    // We use 4 MB pages (PSE) to keep this simple:
    //   - No page tables needed — one level only.
    //   - Each PDE covers exactly 4 MB of physical memory.
    //   - Identity-mapping 64 MB = 16 entries.
    //
    // The directory itself must be 4 KB aligned.
    // We declare it as a global so the linker places it in BSS (aligned).

    class PageDirectory
    {
    public:
        // 1024 page directory entries.
        // Aligned to 4096 bytes — required by CR3.
        myos::common::uint32_t entries[1024] __attribute__((aligned(4096)));

        PageDirectory();

        // Identity-map [0, bytes) using 4 MB pages.
        void identityMap(myos::common::uint32_t bytes);

        // Load this directory into CR3.
        void load();

        // Set CR0 bit 31 (PG) and CR4 bit 4 (PSE) to enable paging + 4MB pages.
        static void enable();

        // Is paging currently active?
        static bool isEnabled();
    };

} // namespace myos

#endif