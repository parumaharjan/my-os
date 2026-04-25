#ifndef __MYOS__SCHEDULER_H
#define __MYOS__SCHEDULER_H

#include <common/types.h>
#include <common/cpustate.h>

#define MAX_PROCS   16
#define MAX_THREADS 32
#define STACK_SIZE  4096
#define THREAD_STACK_SIZE 4096

namespace myos
{
    // ── Process ──────────────────────────────────────────────────────────────
    struct Process
    {
        myos::common::uint8_t  stack[STACK_SIZE];
        CPUState*              cpustate;
        myos::common::uint32_t pid;
        bool                   active;
        char                   name[32];
    };

    // ── Thread ───────────────────────────────────────────────────────────────
    // A thread belongs to a process (ownerPid).
    // It has its own stack and CPUState so it can be scheduled independently.
    // If the owner process is killed, all its threads are killed too.
    struct Thread
    {
        myos::common::uint8_t  stack[THREAD_STACK_SIZE];
        CPUState*              cpustate;
        myos::common::uint32_t tid;
        myos::common::uint32_t ownerPid;  // which process owns this thread
        bool                   active;
        char                   name[32];
    };

    // ── Scheduler ────────────────────────────────────────────────────────────
    class Scheduler
    {
    public:
        Process  procs[MAX_PROCS];
        int      numProcs;
        int      current;           // current process index
        myos::common::uint32_t nextPid;

        Thread   threads[MAX_THREADS];
        int      numThreads;
        int      currentThread;     // current thread index
        myos::common::uint32_t nextTid;

        // Alternates between scheduling a process and a thread each tick.
        // false = schedule process next, true = schedule thread next
        bool threadTurn;

        static Scheduler* active;

        Scheduler();

        // ── Process API ──────────────────────────────────────────────────────
        Process* createProcess(void (*entry)(), const char* name);
        void     killProcess(myos::common::uint32_t pid);

        // ── Thread API ───────────────────────────────────────────────────────
        // ownerPid must be an active process.
        Thread*  createThread(myos::common::uint32_t ownerPid,
                              void (*entry)(),
                              const char* name);
        void     killThread(myos::common::uint32_t tid);

        // ── Called from timer IRQ ────────────────────────────────────────────
        CPUState* schedule(CPUState* cpustate);

    private:
        // Shared stack setup used by both createProcess and createThread.
        void setupStack(CPUState*& cpustate,
                        myos::common::uint8_t* stackBase,
                        myos::common::uint32_t stackSize,
                        void (*entry)());

        CPUState* scheduleProcess(CPUState* cpustate);
        CPUState* scheduleThread(CPUState* cpustate);
    };

} // namespace myos

#endif