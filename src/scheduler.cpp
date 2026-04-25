#include <scheduler.h>
#include <processes.h>
#include <memorymanagement.h>

namespace myos
{
    Scheduler* Scheduler::active = 0;

    // ── Constructor ──────────────────────────────────────────────────────────
    Scheduler::Scheduler()
        : numProcs(0), current(-1), nextPid(1),
          numThreads(0), currentThread(-1), nextTid(1),
          threadTurn(false)
    {
        for(int i = 0; i < MAX_PROCS;   i++) procs[i].active   = false;
        for(int i = 0; i < MAX_THREADS; i++) threads[i].active = false;
        active = this;
    }

    // ── Shared stack setup ───────────────────────────────────────────────────
    // Places a fake CPUState at the top of the given stack so the scheduler
    // can "restore" into this function as if it was interrupted mid-run.
    void Scheduler::setupStack(CPUState*& cpustate,
                               myos::common::uint8_t* stackBase,
                               myos::common::uint32_t stackSize,
                               void (*entry)())
    {
        myos::common::uint8_t* stackTop = stackBase + stackSize;
        stackTop -= sizeof(CPUState);
        cpustate = (CPUState*)stackTop;

        myos::common::uint8_t* s = (myos::common::uint8_t*)cpustate;
        for(myos::common::uint32_t j = 0; j < sizeof(CPUState); j++) s[j] = 0;

        cpustate->eip    = (myos::common::uint32_t)entry;
        cpustate->cs     = 0x10;   // kernel code segment
        cpustate->eflags = 0x202;  // interrupts enabled
        cpustate->ds     = 0x18;   // kernel data segment
        cpustate->es     = 0x18;
        cpustate->fs     = 0x18;
        cpustate->gs     = 0x18;
    }

    // ── Process API ──────────────────────────────────────────────────────────
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

        setupStack(p->cpustate, p->stack, STACK_SIZE, entry);

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
                if(current == i) current = -1;

                // Kill all threads that belong to this process
                for(int t = 0; t < MAX_THREADS; t++)
                    if(threads[t].active && threads[t].ownerPid == pid)
                    {
                        threads[t].active = false;
                        numThreads--;
                        if(currentThread == t) currentThread = -1;
                    }
                return;
            }
        }
    }

    // ── Thread API ───────────────────────────────────────────────────────────
    Thread* Scheduler::createThread(myos::common::uint32_t ownerPid,
                                    void (*entry)(),
                                    const char* name)
    {
        // Owner process must exist
        bool found = false;
        for(int i = 0; i < MAX_PROCS; i++)
            if(procs[i].active && procs[i].pid == ownerPid)
            { found = true; break; }
        if(!found) return 0;

        int slot = -1;
        for(int i = 0; i < MAX_THREADS; i++)
            if(!threads[i].active) { slot = i; break; }
        if(slot == -1) return 0;

        Thread* t = &threads[slot];
        t->tid      = nextTid++;
        t->ownerPid = ownerPid;
        t->active   = true;

        int i = 0;
        for(; i < 31 && name[i]; i++) t->name[i] = name[i];
        t->name[i] = '\0';

        setupStack(t->cpustate, t->stack, THREAD_STACK_SIZE, entry);

        numThreads++;
        return t;
    }
void Scheduler::killThread(myos::common::uint32_t tid)
{
    for(int i = 0; i < MAX_THREADS; i++)
    {
        if(threads[i].active && threads[i].tid == tid)
        {
            // Free thread's memory before marking dead
            if(threads[i].tid == 1 && threadA_mem)
            {
                MemoryManager::active->free(threadA_mem);
                threadA_mem = 0;
            }
            else if(threads[i].tid == 2 && threadB_mem)
            {
                MemoryManager::active->free(threadB_mem);
                threadB_mem = 0;
            }

            threads[i].active = false;
            numThreads--;
            if(currentThread == i) currentThread = -1;

            // ── Show [DEAD] on status bar ──────────────────────
            myos::common::uint16_t* V = (myos::common::uint16_t*)0xb8000;
            int col = (threads[i].tid == 1) ? 42 : 56;
            const char* msg = "[DEAD]  ";
            for(int j = 0; msg[j]; j++)
                V[24*80 + col + j] = (0x0C << 8)
                                   | (myos::common::uint8_t)msg[j];

            return;
        }
    }
}

    // ── schedule() — called from timer IRQ ───────────────────────────────────
    // Alternates between process and thread scheduling each tick.
    // This means processes and threads share CPU time fairly.
    CPUState* Scheduler::schedule(CPUState* cpustate)
    {
        // If we have threads, alternate every tick
        if(numThreads > 0)
        {
            threadTurn = !threadTurn;
            if(threadTurn)
                return scheduleThread(cpustate);
        }
        return scheduleProcess(cpustate);
    }

    // ── Round-robin across processes ─────────────────────────────────────────
    CPUState* Scheduler::scheduleProcess(CPUState* cpustate)
    {
        if(numProcs == 0) return cpustate;

        // Save current process state
        if(current >= 0 && procs[current].active)
            procs[current].cpustate = cpustate;

        // Find next active process
        int tries = MAX_PROCS;
        do {
            current = (current + 1) % MAX_PROCS;
            tries--;
        } while(!procs[current].active && tries > 0);

        if(tries == 0) return cpustate;
        return procs[current].cpustate;
    }

    // ── Round-robin across threads ───────────────────────────────────────────
    CPUState* Scheduler::scheduleThread(CPUState* cpustate)
    {
        if(numThreads == 0) return cpustate;

        // Save current thread state
        if(currentThread >= 0 && threads[currentThread].active)
            threads[currentThread].cpustate = cpustate;

        // Find next active thread whose owner process is still alive
        int tries = MAX_THREADS;
        do {
            currentThread = (currentThread + 1) % MAX_THREADS;
            tries--;

            if(!threads[currentThread].active) continue;

            // Check owner is still alive
            bool ownerAlive = false;
            for(int i = 0; i < MAX_PROCS; i++)
                if(procs[i].active &&
                   procs[i].pid == threads[currentThread].ownerPid)
                { ownerAlive = true; break; }

            // Owner died — auto-kill this thread
            if(!ownerAlive)
            {
                threads[currentThread].active = false;
                numThreads--;
                if(currentThread == currentThread) currentThread = -1;
                continue;
            }
            break;
        } while(tries > 0);

        if(tries == 0) return cpustate;
        return threads[currentThread].cpustate;
    }

} // namespace myos