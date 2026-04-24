// src/scheduler.cpp
#include "scheduler.h"
#include "common/types.h"

Scheduler* activeScheduler = 0;

Scheduler::Scheduler() : numProcesses(0), currentProcess(-1), nextPid(1) {
    for (int i = 0; i < MAX_PROCESSES; i++)
        processes[i].active = false;
    activeScheduler = this;
}

Process* Scheduler::CreateProcess(void (*entrypoint)(), const char* name) {
    if (numProcesses >= MAX_PROCESSES) return 0;

    // Find free slot
    int slot = -1;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (!processes[i].active) { slot = i; break; }
    }
    if (slot == -1) return 0;

    Process* p = &processes[slot];
    p->pid = nextPid++;
    p->active = true;
    p->timeSlice = 3; // 3 ticks per slice

    // Copy name
    for (int i = 0; i < 31 && name[i]; i++) p->name[i] = name[i];
    p->name[31] = 0;

    // Set up fake CPU state at top of stack
    uint8_t* stackTop = p->stack + STACK_SIZE;
    stackTop -= sizeof(CPUState);
    p->cpustate = (CPUState*)stackTop;

    // Zero out, then set EIP and flags
    uint8_t* s = (uint8_t*)p->cpustate;
    for (size_t i = 0; i < sizeof(CPUState); i++) s[i] = 0;
    p->cpustate->eip    = (uint32_t)entrypoint;
    p->cpustate->cs     = 0x08; // kernel code segment
    p->cpustate->eflags = 0x202; // interrupts enabled

    numProcesses++;
    return p;
}

void Scheduler::KillProcess(uint32_t pid) {
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (processes[i].active && processes[i].pid == pid) {
            processes[i].active = false;
            numProcesses--;
            return;
        }
    }
}

CPUState* Scheduler::Schedule(CPUState* cpustate) {
    if (numProcesses == 0) return cpustate;

    // Save state of current process
    if (currentProcess >= 0 && processes[currentProcess].active)
        processes[currentProcess].cpustate = cpustate;

    // Round-robin: find next active process
    int tries = MAX_PROCESSES;
    do {
        currentProcess = (currentProcess + 1) % MAX_PROCESSES;
        tries--;
    } while (!processes[currentProcess].active && tries > 0);

    if (tries == 0) return cpustate; // No active process found
    return processes[currentProcess].cpustate;
}