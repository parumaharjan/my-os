#include <common/types.h>
#include <common/cpustate.h>
#include <gdt.h>
#include <hardwarecommunication/interrupts.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <scheduler.h>
#include <memorymanagement.h>
#include <shell.h>
#include <processes.h>
#include <vga.h>

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;


// ===================== SCHEDULER GLUE =====================

bool schedulerEnabled = false;

CPUState* scheduleNext(CPUState* cpustate)
{
    if(Scheduler::active == 0) return cpustate;

    CPUState* next = Scheduler::active->schedule(cpustate);

    // ── Scheduling trail on row 22 ──────────────────────────────
    static int col = 0;
    int cur = Scheduler::active->current;
    if(cur >= 0 && Scheduler::active->procs[cur].active)
    {
        uint16_t* V = (uint16_t*)0xb8000;
        const char* name = Scheduler::active->procs[cur].name;

        // Each task gets its own color
        uint8_t colors[MAX_PROCS];
        for(int i = 0; i < MAX_PROCS; i++) colors[i] = 0x0F;
        colors[0] = 0x0A; // task-a green
        colors[1] = 0x0C; // task-b red
        colors[2] = 0x0B; // task-c cyan
        uint8_t color = colors[cur];

        // Write name
        for(int j = 0; name[j] && col < 77; j++, col++)
            V[22*80 + col] = ((uint16_t)color << 8) | (uint8_t)name[j];

        // Arrow
        if(col < 77)
        {
            V[22*80 + col++] = (0x08 << 8) | '-';
            V[22*80 + col++] = (0x08 << 8) | '>';
        }

        // Row full — clear and restart
        if(col >= 77)
        {
            for(int i = 0; i < 80; i++)
                V[22*80 + i] = (0x08 << 8) | ' ';
            col = 0;
        }
    }

    return next;
}

// ===================== KERNEL MAIN =====================

extern "C" void kernelMain(const void* multiboot_structure, uint32_t)
{
    // VGA / output already handled elsewhere

    // ===================== 1. GDT =====================
    GlobalDescriptorTable gdt;

    // ===================== 2. HEAP =====================
    MemoryManager memman(10 * 1024 * 1024, 1 * 1024 * 1024);

    // ===================== 3. INTERRUPTS =====================
    InterruptManager interrupts(0x20, &gdt);

    // ===================== 4. SCHEDULER =====================
    Scheduler scheduler;

    scheduler.createProcess(taskA_func, "task-a");
    scheduler.createProcess(taskB_func, "task-b");
    scheduler.createProcess(taskC_func, "task-c");

    // ===================== 5. DRIVERS =====================
    DriverManager drvManager;

    Shell shell(&scheduler);

    KeyboardDriver keyboard(&interrupts, &shell);
   // MouseDriver mouse(&interrupts, &shell);

    drvManager.AddDriver(&keyboard);
   //  drvManager.AddDriver(&mouse);

    drvManager.ActivateAll();
    interrupts.Activate();

    // ===================== 6. ENABLE SCHEDULER =====================
    schedulerEnabled = true;

    // ===================== 7. START SHELL =====================
    shell.start();

    // ===================== 8. IDLE LOOP =====================
    while(1)
        asm("hlt");
}