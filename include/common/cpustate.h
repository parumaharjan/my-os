#ifndef __MYOS__COMMON__CPUSTATE_H
#define __MYOS__COMMON__CPUSTATE_H

#include <common/types.h>

struct CPUState
{
    myos::common::uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
    myos::common::uint32_t error_code;
    myos::common::uint32_t eip, cs, eflags, esp, ss;
} __attribute__((packed));

#endif