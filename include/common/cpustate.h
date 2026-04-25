#ifndef __MYOS__COMMON__CPUSTATE_H
#define __MYOS__COMMON__CPUSTATE_H

#include <common/types.h>

struct CPUState
{
    myos::common::uint32_t gs, fs, es, ds;
    myos::common::uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax;
    myos::common::uint32_t eip, cs, eflags;
} __attribute__((packed));

#endif