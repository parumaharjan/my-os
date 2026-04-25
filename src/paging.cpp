#include <paging.h>

using namespace myos;
using namespace myos::common;

// ── How context switching + paging fits together ─────────────────────────────
//
// Your GDT segments start at base 0 and cover 64 MB.
// Your processes run at virtual addresses == physical addresses (flat model).
// So we identity-map: virtual address X → physical address X for all X < 64 MB.
// After paging is enabled the CPU translates every address through the page
// directory, but since virt == phys the kernel/processes see no difference.
//
// We use 4 MB pages (PSE — Page Size Extension) so we need:
//   - CR4 bit 4 set (PSE enabled)
//   - Each PDE bit 7 set (PS=1, meaning this PDE maps 4 MB directly)
//   - No page tables — one level only
//   - 64 MB / 4 MB = 16 PDEs needed
//
// Page Directory Entry format for 4 MB page (PSE):
//   bits 31-22 : physical base address of the 4 MB frame (top 10 bits)
//   bit  7     : PS = 1 (4 MB page)
//   bit  2     : U/S = 0 (supervisor only)
//   bit  1     : R/W = 1 (read/write)
//   bit  0     : P   = 1 (present)
//   → flags = 0x83 (Present + RW + 4MB page)

PageDirectory::PageDirectory()
{
    // Mark all 1024 entries as not present
    for(int i = 0; i < 1024; i++)
        entries[i] = 0x00000002; // not present, RW
}

void PageDirectory::identityMap(uint32_t bytes)
{
    // Round up to next 4 MB boundary
    uint32_t pages = (bytes + (4*1024*1024 - 1)) / (4*1024*1024);
    if(pages > 1024) pages = 1024;

    for(uint32_t i = 0; i < pages; i++)
    {
        // Physical base address of this 4 MB frame
        uint32_t physBase = i * 4 * 1024 * 1024;

        // PDE: base | PS | RW | Present
        entries[i] = physBase | 0x83;
    }
}

void PageDirectory::load()
{
    // CR3 = physical address of page directory (must be 4K aligned)
    asm volatile("mov %0, %%cr3" :: "r"((uint32_t)entries) : "memory");
}

void PageDirectory::enable()
{
    // Step 1: Enable PSE (4 MB pages) in CR4
    uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    cr4 |= 0x00000010; // bit 4 = PSE
    asm volatile("mov %0, %%cr4" :: "r"(cr4) : "memory");

    // Step 2: Enable paging in CR0
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // bit 31 = PG
    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

bool PageDirectory::isEnabled()
{
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    return (cr0 & 0x80000000) != 0;
}