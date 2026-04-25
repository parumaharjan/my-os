#include <scheduler.h>

namespace myos
{
    Scheduler* Scheduler::active = 0;

    Scheduler::Scheduler() : numProcs(0), current(-1), nextPid(1)
    {
        for(int i = 0; i < MAX_PROCS; i++) procs[i].active = false;
        active = this;
    }

    Process* Scheduler::createProcess(void (*entry)(), const char* name)
    {
        int slot = -1;
        for(int i = 0; i < MAX_PROCS; i++)
            if(!procs[i].active) { slot = i; break; }
        if(slot == -1) return 0;

        Process* p = &procs[slot];
        p->pid    = nextPid++;
        p->active = true;

        int i = 0;
        for(; i < 31 && name[i]; i++) p->name[i] = name[i];
        p->name[i] = '\0';

        // Place CPUState at top of stack
        // Stack grows downward — we need room for CPUState AND
        // the segment registers that int_bottom pushes BEFORE pusha.
        // Total frame = sizeof(CPUState) bytes.
        myos::common::uint8_t* stackTop = p->stack + STACK_SIZE;
        stackTop -= sizeof(CPUState);
        p->cpustate = (CPUState*)stackTop;

        myos::common::uint8_t* s = (myos::common::uint8_t*)p->cpustate;
        for(myos::common::uint32_t j = 0; j < sizeof(CPUState); j++) s[j] = 0;

        p->cpustate->eip    = (myos::common::uint32_t)entry;
        p->cpustate->cs     = 0x10;   // ← was 0x08, code segment is at 0x10
        p->cpustate->eflags = 0x202;
        p->cpustate->ds     = 0x18;   // ← was 0x10, data segment is at 0x18
        p->cpustate->es     = 0x18;
        p->cpustate->fs     = 0x18;
        p->cpustate->gs     = 0x18;

        numProcs++;
        return p;
    }

    void Scheduler::killProcess(myos::common::uint32_t pid)
    {
        for(int i = 0; i < MAX_PROCS; i++)
        {
            if(procs[i].active && procs[i].pid == pid)
            {
                procs[i].active = false;
                numProcs--;
                // Reset current so scheduler doesn't save into dead slot
                if(current == i) current = -1;
                return;
            }
        }
    }

    CPUState* Scheduler::schedule(CPUState* cpustate)
    {
        if(numProcs == 0) return cpustate;

        // Save current process state
        if(current >= 0 && procs[current].active)
            procs[current].cpustate = cpustate;

        // Round-robin: find next active process
        int tries = MAX_PROCS;
        do {
            current = (current + 1) % MAX_PROCS;
            tries--;
        } while(!procs[current].active && tries > 0);

        if(tries == 0) return cpustate;
        return procs[current].cpustate;
    }
}