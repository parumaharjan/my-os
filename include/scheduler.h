// include/scheduler.h
#ifndef __SCHEDULER_H
#define __SCHEDULER_H
#include "common/types.h"
#include "gdt.h"

#define MAX_PROCESSES 64
#define STACK_SIZE    4096

struct CPUState {
    uint32_t eax, ebx, ecx, edx, esi, edi, ebp;
    uint32_t error_code;
    uint32_t eip, cs, eflags, esp, ss;
} __attribute__((packed));

struct Process {
    uint8_t  stack[STACK_SIZE];
    CPUState* cpustate;
    uint32_t pid;
    bool     active;
    char     name[32];
    uint32_t timeSlice; // remaining ticks
};

class Scheduler {
    Process  processes[MAX_PROCESSES];
    int      numProcesses;
    int      currentProcess;
    uint32_t nextPid;
public:
    Scheduler();
    Process* CreateProcess(void (*entrypoint)(), const char* name);
    void     KillProcess(uint32_t pid);
    // Called by timer IRQ — returns new esp
    CPUState* Schedule(CPUState* cpustate);
};

extern Scheduler* activeScheduler;

#endif