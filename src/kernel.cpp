#include <common/types.h>
#include <gdt.h>
#include <hardwarecommunication/interrupts.h>
#include <drivers/driver.h>
#include <drivers/keyboard.h>
#include <drivers/mouse.h>
#include <common/cpustate.h>  

using namespace myos;
using namespace myos::common;
using namespace myos::drivers;
using namespace myos::hardwarecommunication;


// ===================== VGA / PRINT =====================

static uint16_t* VideoMemory = (uint16_t*)0xb8000;
static uint8_t cursorX = 0, cursorY = 0;

void clearScreen()
{
    for(int i = 0; i < 80*25; i++)
        VideoMemory[i] = (0x0F << 8) | ' ';
    cursorX = 0;
    cursorY = 0;
}

void scrollUp()
{
    for(int y = 0; y < 24; y++)
        for(int x = 0; x < 80; x++)
            VideoMemory[80*y + x] = VideoMemory[80*(y+1) + x];
    for(int x = 0; x < 80; x++)
        VideoMemory[80*24 + x] = (0x0F << 8) | ' ';
    cursorY = 24;
}

void putChar(char c, uint8_t color = 0x0F)
{
    if(c == '\n')
    {
        cursorX = 0;
        cursorY++;
    }
    else if(c == '\b')
    {
        if(cursorX > 0)
        {
            cursorX--;
            VideoMemory[80*cursorY + cursorX] = (color << 8) | ' ';
        }
    }
    else
    {
        VideoMemory[80*cursorY + cursorX] = (color << 8) | (uint8_t)c;
        cursorX++;
        if(cursorX >= 80) { cursorX = 0; cursorY++; }
    }

    if(cursorY >= 25)
        scrollUp();
}

void printf(const char* str, uint8_t color = 0x0F)
{
    for(int i = 0; str[i] != '\0'; i++)
        putChar(str[i], color);
}

void printInt(uint32_t n)
{
    if(n == 0) { putChar('0'); return; }
    char buf[12];
    int i = 0;
    while(n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    for(int j = i-1; j >= 0; j--) putChar(buf[j]);
}

void printfHex(uint8_t key)
{
    const char* hex = "0123456789ABCDEF";
    putChar(hex[(key >> 4) & 0xF]);
    putChar(hex[key & 0xF]);
}


// ===================== STRING UTILS =====================

int strlen(const char* s)
{
    int i = 0;
    while(s[i]) i++;
    return i;
}

bool streq(const char* a, const char* b)
{
    for(int i = 0; ; i++)
    {
        if(a[i] != b[i]) return false;
        if(!a[i]) return true;
    }
}

bool startsWith(const char* str, const char* prefix)
{
    for(int i = 0; prefix[i]; i++)
        if(str[i] != prefix[i]) return false;
    return true;
}

void strcpy(char* dst, const char* src)
{
    int i = 0;
    while((dst[i] = src[i])) i++;
}

// Split command into verb + args (modifies cmd in place)
char* splitArgs(char* cmd)
{
    int i = 0;
    while(cmd[i] && cmd[i] != ' ') i++;
    if(!cmd[i]) return cmd + i;   // no args
    cmd[i] = '\0';
    return cmd + i + 1;
}


// ===================== MEMORY MANAGER =====================

struct MemoryChunk
{
    MemoryChunk* next;
    MemoryChunk* prev;
    bool         allocated;
    uint32_t     size;
};

class MemoryManager
{
public:
    MemoryChunk* first;
    static MemoryManager* active;

    MemoryManager(uint32_t start, uint32_t size)
    {
        active = this;
        if(size < sizeof(MemoryChunk)) { first = 0; return; }
        first = (MemoryChunk*)start;
        first->allocated = false;
        first->prev = 0;
        first->next = 0;
        first->size = size - sizeof(MemoryChunk);
    }

    void* malloc(uint32_t size)
    {
        MemoryChunk* result = 0;
        for(MemoryChunk* c = first; c != 0; c = c->next)
        {
            if(!c->allocated && c->size >= size)
            { result = c; break; }
        }
        if(!result) return 0;

        // Split chunk if large enough
        if(result->size >= size + sizeof(MemoryChunk) + 1)
        {
            MemoryChunk* tmp = (MemoryChunk*)((uint32_t)result + sizeof(MemoryChunk) + size);
            tmp->allocated = false;
            tmp->size = result->size - size - sizeof(MemoryChunk);
            tmp->prev = result;
            tmp->next = result->next;
            if(tmp->next) tmp->next->prev = tmp;
            result->next = tmp;
            result->size = size;
        }
        result->allocated = true;
        return (void*)((uint32_t)result + sizeof(MemoryChunk));
    }

    void free(void* ptr)
    {
        MemoryChunk* chunk = (MemoryChunk*)((uint32_t)ptr - sizeof(MemoryChunk));
        chunk->allocated = false;
        // Merge with prev
        if(chunk->prev && !chunk->prev->allocated)
        {
            chunk->prev->next = chunk->next;
            chunk->prev->size += chunk->size + sizeof(MemoryChunk);
            if(chunk->next) chunk->next->prev = chunk->prev;
            chunk = chunk->prev;
        }
        // Merge with next
        if(chunk->next && !chunk->next->allocated)
        {
            chunk->size += chunk->next->size + sizeof(MemoryChunk);
            chunk->next = chunk->next->next;
            if(chunk->next) chunk->next->prev = chunk;
        }
    }
};

MemoryManager* MemoryManager::active = 0;

void* operator new(uint32_t size)   { return MemoryManager::active->malloc(size); }
void* operator new[](uint32_t size) { return MemoryManager::active->malloc(size); }
void  operator delete(void* ptr)    { MemoryManager::active->free(ptr); }
void  operator delete[](void* ptr)  { MemoryManager::active->free(ptr); }


// ===================== PAGING =====================

// Page frame allocator using a simple bitmap
/* class PageFrameAllocator
{
    uint8_t* bitmap;
    uint32_t totalPages;
    uint32_t firstFree;
public:
    PageFrameAllocator(uint32_t bitmapAddr, uint32_t memBytes)
    {
        bitmap     = (uint8_t*)bitmapAddr;
        totalPages = memBytes / 4096;
        firstFree  = 0;
        uint32_t bytes = (totalPages + 7) / 8;
        for(uint32_t i = 0; i < bytes; i++) bitmap[i] = 0;
    }

    uint32_t alloc()
    {
        for(uint32_t i = firstFree; i < totalPages; i++)
        {
            if(!(bitmap[i/8] & (1 << (i%8))))
            {
                bitmap[i/8] |= (1 << (i%8));
                firstFree = i + 1;
                return i * 4096;
            }
        }
        return 0; // OOM
    }

    void free(uint32_t physAddr)
    {
        uint32_t i = physAddr / 4096;
        bitmap[i/8] &= ~(1 << (i%8));
        if(i < firstFree) firstFree = i;
    }
};

// Simple page directory — identity maps a range
class PageDirectory
{
    uint32_t entries[1024]; // stored in BSS, must be 4KB-aligned ideally
public:
    PageDirectory()
    {
        for(int i = 0; i < 1024; i++)
            entries[i] = 0x00000002; // not present, RW
    }

    // Identity-map physical pages [start, start+size)
    void identityMap(uint32_t start, uint32_t size)
    {
        for(uint32_t addr = start; addr < start + size; addr += 4096)
            mapPage(addr, addr, true, false);
    }

    void mapPage(uint32_t virt, uint32_t phys, bool rw, bool user)
    {
        uint32_t pdi = virt >> 22;
        uint32_t pti = (virt >> 12) & 0x3FF;

        uint32_t* table;
        if(!(entries[pdi] & 1))
        {
            // Allocate a static page table (simple: use a static array bank)
            // For a real OS use PageFrameAllocator; here we use a fixed pool
            static uint32_t tablePool[1024][1024];
            static int tableIdx = 0;
            table = tablePool[tableIdx++];
            for(int i = 0; i < 1024; i++) table[i] = 0x00000002;
            entries[pdi] = (uint32_t)table | 0x01 | (rw ? 0x02 : 0) | (user ? 0x04 : 0);
        }
        else
        {
            table = (uint32_t*)(entries[pdi] & ~0xFFF);
        }
        table[pti] = (phys & ~0xFFF) | 0x01 | (rw ? 0x02 : 0) | (user ? 0x04 : 0);
    }

    void load()
    {
        asm volatile("mov %0, %%cr3" :: "r"(entries) : "memory");
    }

    static void enable()
    {
        uint32_t cr0;
        asm volatile("mov %%cr0, %0" : "=r"(cr0));
        cr0 |= 0x80000000;
        asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
    }
};
*/

// ===================== SCHEDULER (Round-Robin) =====================

#define MAX_PROCS   16
#define STACK_SIZE  4096

struct Process
{
    uint8_t   stack[STACK_SIZE];
    CPUState* cpustate;
    uint32_t  pid;
    bool      active;
    char      name[32];
};

class Scheduler
{
public:
    Process  procs[MAX_PROCS];
    int      numProcs;
    int      current;
    uint32_t nextPid;

    static Scheduler* active;

    Scheduler() : numProcs(0), current(-1), nextPid(1)
    {
        for(int i = 0; i < MAX_PROCS; i++) procs[i].active = false;
        active = this;
    }

    Process* createProcess(void (*entry)(), const char* name)
    {
        int slot = -1;
        for(int i = 0; i < MAX_PROCS; i++)
            if(!procs[i].active) { slot = i; break; }
        if(slot == -1) return 0;

        Process* p = &procs[slot];
        p->pid    = nextPid++;
        p->active = true;

        // Copy name
        int i = 0;
        for(; i < 31 && name[i]; i++) p->name[i] = name[i];
        p->name[i] = '\0';

        // Place fake CPU state at top of process stack
        uint8_t* stackTop = p->stack + STACK_SIZE;
        stackTop -= sizeof(CPUState);
        p->cpustate = (CPUState*)stackTop;

        // Zero out
        uint8_t* s = (uint8_t*)p->cpustate;
        for(uint32_t j = 0; j < sizeof(CPUState); j++) s[j] = 0;

        p->cpustate->eip    = (uint32_t)entry;
        p->cpustate->cs     = 0x08;
        p->cpustate->eflags = 0x202; // IF set

        numProcs++;
        return p;
    }

    void killProcess(uint32_t pid)
    {
        for(int i = 0; i < MAX_PROCS; i++)
        {
            if(procs[i].active && procs[i].pid == pid)
            {
                procs[i].active = false;
                numProcs--;
                return;
            }
        }
    }

    // Called from timer IRQ — returns new stack pointer
    CPUState* schedule(CPUState* cpustate)
    {
        if(numProcs == 0) return cpustate;

        // Save current process state
        if(current >= 0 && procs[current].active)
            procs[current].cpustate = cpustate;

        // Find next active process (round-robin)
        int tries = MAX_PROCS;
        do {
            current = (current + 1) % MAX_PROCS;
            tries--;
        } while(!procs[current].active && tries > 0);

        if(tries == 0) return cpustate;
        return procs[current].cpustate;
    }
};

Scheduler* Scheduler::active = 0;


// ===================== SHELL =====================

#define BUF_SIZE 256

class Shell : public KeyboardEventHandler
{
    char     buf[BUF_SIZE];
    int      bufPos;
    Scheduler* sched;

public:
    Shell(Scheduler* s) : bufPos(0), sched(s)
    {
        for(int i = 0; i < BUF_SIZE; i++) buf[i] = '\0';
    }

    void start()
    {
        clearScreen();
        printf("MyOS Shell  [type 'help']\n", 0x0A); // green header
        printPrompt();
    }

    void OnKeyDown(char c) override
    {
        if(c == '\n')
        {
            putChar('\n');
            buf[bufPos] = '\0';
            if(bufPos > 0) execute(buf);
            bufPos = 0;
            printPrompt();
        }
        else if(c == '\b')
        {
            if(bufPos > 0) { bufPos--; putChar('\b'); }
        }
        else if(bufPos < BUF_SIZE - 1)
        {
            buf[bufPos++] = c;
            putChar(c);
        }
    }

private:
    void printPrompt()
    {
        printf("\nmyos", 0x0B);  // cyan
        printf("> ", 0x0F);
    }

    void execute(char* cmd)
    {
        // Copy to a scratch buffer so splitArgs can mutate it
        char scratch[BUF_SIZE];
        strcpy(scratch, cmd);
        char* args = splitArgs(scratch);

        if(streq(scratch, "help"))    { cmdHelp();        return; }
        if(streq(scratch, "clear"))   { cmdClear();       return; }
        if(streq(scratch, "echo"))    { cmdEcho(args);    return; }
        if(streq(scratch, "ps"))      { cmdPs();          return; }
        if(streq(scratch, "kill"))    { cmdKill(args);    return; }
        if(streq(scratch, "version")) { cmdVersion();     return; }
        if(streq(scratch, "meminfo")) { cmdMeminfo();     return; }
        if(streq(scratch, "reboot"))  { cmdReboot();      return; }

        printf("\nUnknown: ", 0x0C); // red
        printf(scratch, 0x0C);
        printf("  (type 'help')", 0x08);
    }

    void cmdHelp()
    {
        printf("\n--- Commands ---\n", 0x0E); // yellow
        printf("  help      show this list\n", 0x0F);
        printf("  clear     clear the screen\n", 0x0F);
        printf("  echo <x>  print text\n", 0x0F);
        printf("  ps        list processes\n", 0x0F);
        printf("  kill <pid> terminate a process\n", 0x0F);
        printf("  meminfo   heap fragmentation info\n", 0x0F);
        printf("  version   kernel version\n", 0x0F);
        printf("  reboot    cold reboot via keyboard ctrl\n", 0x0F);
    }

    void cmdClear()
    {
        clearScreen();
        printf("MyOS Shell  [type 'help']\n", 0x0A);
    }

    void cmdEcho(char* args)
    {
        putChar('\n');
        printf(args, 0x0F);
    }

    void cmdPs()
    {
        printf("\nPID  NAME\n", 0x0E);
        printf("---  ----\n", 0x08);
        if(!sched) { printf("no scheduler\n", 0x0C); return; }
        bool any = false;
        for(int i = 0; i < MAX_PROCS; i++)
        {
            if(sched->procs[i].active)
            {
                printInt(sched->procs[i].pid);
                printf("    ", 0x0F);
                printf(sched->procs[i].name, 0x0F);
                putChar('\n');
                any = true;
            }
        }
        if(!any) printf("(no processes)\n", 0x08);
    }

    void cmdKill(char* args)
    {
        if(!args || !args[0]) { printf("\nusage: kill <pid>\n", 0x0C); return; }
        uint32_t pid = 0;
        for(int i = 0; args[i] >= '0' && args[i] <= '9'; i++)
            pid = pid * 10 + (args[i] - '0');
        if(!sched) return;
        sched->killProcess(pid);
        printf("\nKilled PID ", 0x0E);
        printInt(pid);
        putChar('\n');
    }

    void cmdVersion()
    {
        printf("\nMyOS 0.1  |  kernel built from scratch\n", 0x0B);
        printf("Arch: x86 32-bit  |  Boot: GRUB/Multiboot\n", 0x08);
        printf("Features: GDT, IDT, PIC, keyboard, mouse,\n", 0x08);
        printf("          heap alloc, paging, round-robin sched\n", 0x08);
    }

    void cmdMeminfo()
    {
        if(!MemoryManager::active) { printf("\nno memory manager\n", 0x0C); return; }
        printf("\n--- Heap chunks ---\n", 0x0E);
        uint32_t total = 0, free = 0, used = 0, chunks = 0, frag = 0;
        MemoryChunk* c = MemoryManager::active->first;
        bool lastFree = false;
        while(c)
        {
            chunks++;
            total += c->size;
            if(c->allocated) used += c->size;
            else {
                free += c->size;
                if(lastFree) frag++; // two adjacent free chunks = fragmentation
            }
            lastFree = !c->allocated;
            c = c->next;
        }
        printf("  Chunks : ", 0x0F); printInt(chunks); putChar('\n');
        printf("  Total  : ", 0x0F); printInt(total);  printf(" bytes\n", 0x08);
        printf("  Used   : ", 0x0F); printInt(used);   printf(" bytes\n", 0x08);
        printf("  Free   : ", 0x0F); printInt(free);   printf(" bytes\n", 0x08);
        printf("  Frag   : ", frag ? 0x0C : 0x0A); printInt(frag);
        printf(frag ? " adjacent free chunks (fragmented)\n" : " (no fragmentation)\n", 0x0F);
    }

    void cmdReboot()
    {
        printf("\nRebooting...\n", 0x0C);
        // Triple-fault via keyboard controller pulse
        uint8_t good = 0x02;
        while(good & 0x02)
            asm volatile("inb $0x64, %0" : "=a"(good));
        asm volatile("outb %0, $0x64" :: "a"((uint8_t)0xFE));
        while(1) asm("hlt");
    }
};


// ===================== MOUSE HANDLER =====================

class MouseToConsole : public MouseEventHandler
{
    int8_t x, y;
public:
    MouseToConsole() {}

    virtual void OnActivate()
    {
        x = 40; y = 12;
        VideoMemory[80*y + x] =
            (VideoMemory[80*y + x] & 0x0F00) << 4 |
            (VideoMemory[80*y + x] & 0xF000) >> 4 |
            (VideoMemory[80*y + x] & 0x00FF);
    }

    virtual void OnMouseMove(int xoffset, int yoffset)
    {
        // Erase old cursor
        VideoMemory[80*y + x] =
            (VideoMemory[80*y + x] & 0x0F00) << 4 |
            (VideoMemory[80*y + x] & 0xF000) >> 4 |
            (VideoMemory[80*y + x] & 0x00FF);

        x += xoffset;
        y += yoffset;
        if(x < 0) x = 0; if(x >= 80) x = 79;
        if(y < 0) y = 0; if(y >= 25) y = 24;

        // Draw new cursor
        VideoMemory[80*y + x] =
            (VideoMemory[80*y + x] & 0x0F00) << 4 |
            (VideoMemory[80*y + x] & 0xF000) >> 4 |
            (VideoMemory[80*y + x] & 0x00FF);
    }
};


// ===================== CONSTRUCTORS =====================

typedef void (*constructor)();
extern "C" constructor start_ctors;
extern "C" constructor end_ctors;

extern "C" void callConstructors()
{
    for(constructor* i = &start_ctors; i != &end_ctors; i++)
        (*i)();
}


// ===================== SCHEDULER GLUE (called from interrupts.cpp) =====================
// These are declared extern in interrupts.cpp so the timer IRQ can reach the scheduler
// without interrupts.cpp needing to know about the Scheduler class.

bool schedulerEnabled = false;

// Forward the timer tick to the active scheduler instance.
// The cast is safe: CPUState in interrupts.cpp and in this file are identical structs.
CPUState* scheduleNext(CPUState* cpustate)
{
    if(Scheduler::active == 0) return cpustate;
    return Scheduler::active->schedule(cpustate);
}


// ===================== KERNEL MAIN =====================

extern "C" void kernelMain(const void* multiboot_structure, uint32_t)
{
    // --- 1. GDT ---
    GlobalDescriptorTable gdt;

    // --- 2. Heap (1 MB starting at 10 MB, safely above kernel) ---
    MemoryManager memman(10 * 1024 * 1024, 1 * 1024 * 1024);

    // --- 3. Interrupts ---
    InterruptManager interrupts(0x20, &gdt);

    // --- 4. Paging: identity-map first 16 MB so the kernel keeps working ---
   /* PageDirectory pd;
    pd.identityMap(0, 16 * 1024 * 1024);
    pd.load();
    PageDirectory::enable(); */

    // --- 5. Scheduler ---
    Scheduler scheduler;

    // --- 6. Drivers ---
    DriverManager drvManager;

    Shell shell(&scheduler);
    KeyboardDriver keyboard(&interrupts, &shell);
    drvManager.AddDriver(&keyboard);

   // MouseToConsole mousehandler;
   // MouseDriver mouse(&interrupts, &mousehandler);
    //drvManager.AddDriver(&mouse);

    drvManager.ActivateAll();

    // --- 7. Activate interrupts ---
    interrupts.Activate();

    // --- 8. Enable scheduler (after interrupts are live) ---
    schedulerEnabled = true;

    // --- 9. Start shell ---
    shell.start();

    // --- 9. Idle ---
    while(1) asm("hlt");
}