#ifndef __MYOS__SCHEDULER_H
#define __MYOS__SCHEDULER_H

#include <common/types.h>
#include <common/cpustate.h>

#define MAX_PROCS  16
#define STACK_SIZE 4096

namespace myos
{
    struct Process
    {
        myos::common::uint8_t  stack[STACK_SIZE];
        CPUState*              cpustate;
        myos::common::uint32_t pid;
        bool                   active;
        char                   name[32];
    };

    class Scheduler
    {
    public:
        Process  procs[MAX_PROCS];
        int      numProcs;
        int      current;
        myos::common::uint32_t nextPid;

        static Scheduler* active;

        Scheduler();
        Process* createProcess(void (*entry)(), const char* name);
        void     killProcess(myos::common::uint32_t pid);
        CPUState* schedule(CPUState* cpustate);
    };
}

#endif