#include <shell.h>
#include <vga.h>
#include <memorymanagement.h>
#include <paging.h>

// ===================== STRING UTILS (local to shell) =====================

static bool streq(const char* a, const char* b)
{
    for(int i = 0; ; i++)
    {
        if(a[i] != b[i]) return false;
        if(!a[i]) return true;
    }
}

static void strcpy(char* dst, const char* src)
{
    int i = 0;
    while((dst[i] = src[i])) i++;
}

static char* splitArgs(char* cmd)
{
    int i = 0;
    while(cmd[i] && cmd[i] != ' ') i++;
    if(!cmd[i]) return cmd + i;
    cmd[i] = '\0';
    return cmd + i + 1;
}

// ===================== SHELL =====================

Shell::Shell(myos::Scheduler* s) : bufPos(0), sched(s)
{
    for(int i = 0; i < BUF_SIZE; i++) buf[i] = '\0';
}

void Shell::start()
{
    clearScreen();

    // Draw permanent status bar on row 24
    myos::common::uint16_t* V = (myos::common::uint16_t*)0xb8000;
    const char* bar = "[ A ]      [ B ]      [ C ]      "
                      "[ TA ]     [ TB ]               ";
    for(int i = 0; i < 80 && bar[i]; i++)
        V[24*80 + i] = (0x08 << 8) | (myos::common::uint8_t)bar[i];

    printf("MyOS Shell  [type 'help']\n", 0x0A);
    printPrompt();
}

void Shell::OnKeyDown(char c)
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

void Shell::printPrompt()
{
    printf("\nmyos", 0x0B);
    printf("> ", 0x0F);
}

void Shell::execute(char* cmd)
{
    char scratch[BUF_SIZE];
    strcpy(scratch, cmd);
    char* args = splitArgs(scratch);

    if(streq(scratch, "help"))       { cmdHelp();            return; }
    if(streq(scratch, "clear"))      { cmdClear();           return; }
    if(streq(scratch, "echo"))       { cmdEcho(args);        return; }
    if(streq(scratch, "ps"))         { cmdPs();              return; }
    if(streq(scratch, "threads"))    { cmdThreads();         return; }
    if(streq(scratch, "kill"))       { cmdKill(args);        return; }
    if(streq(scratch, "killthread")) { cmdKillThread(args);  return; }
    if(streq(scratch, "version"))    { cmdVersion();         return; }
    if(streq(scratch, "meminfo"))    { cmdMeminfo();         return; }
    if(streq(scratch, "paging"))     { cmdPaging();          return; }
    if(streq(scratch, "reboot"))     { cmdReboot();          return; }

    printf("\nUnknown: ", 0x0C);
    printf(scratch, 0x0C);
    printf("  (type 'help')", 0x08);
}

// ===================== COMMANDS =====================

void Shell::cmdHelp()
{
    printf("\n--- Commands ---\n", 0x0E);
    printf("  help             show this list\n",             0x0F);
    printf("  clear            clear screen\n",               0x0F);
    printf("  echo <x>         print text\n",                 0x0F);
    printf("  ps               list processes\n",             0x0F);
    printf("  threads          list all threads\n",           0x0F);
    printf("  kill <pid>       terminate a process\n",        0x0F);
    printf("  killthread <tid> terminate a thread\n",         0x0F);
    printf("  meminfo          heap details\n",               0x0F);
    printf("  paging           show CR0/CR3/PSE status\n",   0x0F);
    printf("  version          kernel info\n",                0x0F);
    printf("  reboot           cold reboot\n",                0x0F);
    printf("\n  Row 24 = live tick counters\n",               0x08);
}

void Shell::cmdClear()
{
    clearScreen();
    printf("MyOS Shell  [type 'help']\n", 0x0A);
}

void Shell::cmdEcho(char* args)
{
    putChar('\n');
    printf(args, 0x0F);
}

void Shell::cmdPs()
{
    printf("\nPID  STATUS    NAME\n", 0x0E);
    printf("---  ------    ----\n",  0x08);
    if(!sched) { printf("no scheduler\n", 0x0C); return; }

    bool any = false;
    for(int i = 0; i < MAX_PROCS; i++)
    {
        if(!sched->procs[i].active) continue;

        bool isCur = (i == sched->current);
        myos::common::uint8_t col = isCur ? 0x0A : 0x0F;

        printInt(sched->procs[i].pid);
        printf(isCur ? "    RUNNING   " : "    ready     ", col);
        printf(sched->procs[i].name, col);
        putChar('\n');
        any = true;
    }
    if(!any) printf("(no processes)\n", 0x08);
}

void Shell::cmdThreads()
{
    printf("\nTID  OWNER  STATUS    NAME\n", 0x0E);
    printf("---  -----  ------    ----\n",  0x08);
    if(!sched) { printf("no scheduler\n", 0x0C); return; }

    bool any = false;
    for(int i = 0; i < MAX_THREADS; i++)
    {
        myos::Thread& t = sched->threads[i];
        if(!t.active) continue;

        bool isCur = (i == sched->currentThread);
        myos::common::uint8_t col = isCur ? 0x0A : 0x0F;

        printInt(t.tid);
        printf("    ", 0x0F);
        printInt(t.ownerPid);
        printf(isCur ? "      RUNNING   " : "      ready     ", col);
        printf(t.name, col);
        putChar('\n');
        any = true;
    }
    if(!any) printf("(no threads)\n", 0x08);
}

void Shell::cmdKill(char* args)
{
    if(!args || !args[0]) { printf("\nusage: kill <pid>\n", 0x0C); return; }

    myos::common::uint32_t pid = 0;
    for(int i = 0; args[i] >= '0' && args[i] <= '9'; i++)
        pid = pid * 10 + (args[i] - '0');

    if(!sched) return;

    // Find slot first so we can report name and clear status bar
    int slot = -1;
    for(int i = 0; i < MAX_PROCS; i++)
        if(sched->procs[i].active && sched->procs[i].pid == pid)
        { slot = i; break; }

    if(slot == -1)
    {
        printf("\nNo process with PID ", 0x0C);
        printInt(pid); putChar('\n');
        return;
    }

    char killedName[32];
    strcpy(killedName, sched->procs[slot].name);

    // This also kills all threads owned by this process
    sched->killProcess(pid);

    // Clear that task's column on the status bar (row 24)
    myos::common::uint16_t* V = (myos::common::uint16_t*)0xb8000;
    int clearCol = -1;
    if(streq(killedName, "task-a")) clearCol = 0;
    else if(streq(killedName, "task-b")) clearCol = 10;
    else if(streq(killedName, "task-c")) clearCol = 20;

    if(clearCol >= 0)
    {
        const char* dead = "[dead]    ";
        for(int i = 0; dead[i] && clearCol+i < 80; i++)
            V[24*80 + clearCol + i] = (0x08 << 8)
                                    | (myos::common::uint8_t)dead[i];
    }

    printf("\nKilled PID ", 0x0A);
    printInt(pid);
    printf(" (", 0x08); printf(killedName, 0x08); printf(")\n", 0x08);
    printf("All threads owned by this process also killed.\n", 0x08);
}

void Shell::cmdKillThread(char* args)
{
    if(!args || !args[0])
    { printf("\nusage: killthread <tid>\n", 0x0C); return; }

    myos::common::uint32_t tid = 0;
    for(int i = 0; args[i] >= '0' && args[i] <= '9'; i++)
        tid = tid * 10 + (args[i] - '0');

    if(!sched) return;

    // Find thread so we can report its name
    int slot = -1;
    for(int i = 0; i < MAX_THREADS; i++)
        if(sched->threads[i].active && sched->threads[i].tid == tid)
        { slot = i; break; }

    if(slot == -1)
    {
        printf("\nNo thread with TID ", 0x0C);
        printInt(tid); putChar('\n');
        return;
    }

    char tname[32];
    strcpy(tname, sched->threads[slot].name);
    myos::common::uint32_t ownerPid = sched->threads[slot].ownerPid;

    sched->killThread(tid);

    printf("\nKilled thread TID ", 0x0A);
    printInt(tid);
    printf(" (", 0x08); printf(tname, 0x08);
    printf(") owned by PID ", 0x08);
    printInt(ownerPid);
    printf("\nProcess still running — only this thread stopped.\n", 0x08);
}

void Shell::cmdVersion()
{
    printf("\nMyOS 0.1  |  kernel built from scratch\n", 0x0B);
    printf("Arch: x86 32-bit  |  Boot: GRUB/Multiboot\n", 0x08);
    printf("Features: GDT, IDT, PIC, paging (PSE),\n",    0x08);
    printf("          heap, round-robin, processes+threads\n", 0x08);
}

void Shell::cmdMeminfo()
{
    if(!MemoryManager::active) { printf("\nno memory manager\n", 0x0C); return; }
    printf("\n--- Heap chunks ---\n", 0x0E);

    myos::common::uint32_t total = 0, free = 0, used = 0,
                            chunks = 0, frag = 0;
    MemoryChunk* c = MemoryManager::active->first;
    bool lastFree = false;
    while(c)
    {
        chunks++;
        total += c->size;
        if(c->allocated) used += c->size;
        else {
            free += c->size;
            if(lastFree) frag++;
        }
        lastFree = !c->allocated;
        c = c->next;
    }

    printf("  Chunks : ", 0x0F); printInt(chunks); putChar('\n');
    printf("  Total  : ", 0x0F); printInt(total);  printf(" bytes\n", 0x08);
    printf("  Used   : ", 0x0F); printInt(used);   printf(" bytes\n", 0x08);
    printf("  Free   : ", 0x0F); printInt(free);   printf(" bytes\n", 0x08);
    printf("  Frag   : ", frag ? 0x0C : 0x0A);
    printInt(frag);
    printf(frag ? " adjacent free chunks (fragmented)\n"
                : " (no fragmentation)\n", 0x0F);
}

void Shell::cmdPaging()
{
    printf("\n--- Paging Status ---\n", 0x0E);

    myos::common::uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    bool pagingOn = (cr0 & 0x80000000) != 0;
    printf("  CR0 PG bit   : ", 0x0F);
    printf(pagingOn ? "SET (paging ON)\n" : "CLEAR (paging OFF)\n",
           pagingOn ? 0x0A : 0x0C);

    myos::common::uint32_t cr4;
    asm volatile("mov %%cr4, %0" : "=r"(cr4));
    bool pseOn = (cr4 & 0x00000010) != 0;
    printf("  CR4 PSE bit  : ", 0x0F);
    printf(pseOn ? "SET (4MB pages ON)\n" : "CLEAR\n",
           pseOn ? 0x0A : 0x0C);

    myos::common::uint32_t cr3;
    asm volatile("mov %%cr3, %0" : "=r"(cr3));
    printf("  CR3 (pagedir): 0x", 0x0F);
    for(int i = 28; i >= 0; i -= 4)
        putChar("0123456789ABCDEF"[(cr3 >> i) & 0xF], 0x0B);
    putChar('\n');

    printf("  Page size    : 4 MB per entry (PSE)\n", 0x0F);
    printf("  Mapped range : 0x00000000 - 0x03FFFFFF (64 MB)\n", 0x0F);
    printf("  Mode         : identity map (virt == phys)\n", 0x08);
}

void Shell::cmdReboot()
{
    printf("\nRebooting...\n", 0x0C);
    myos::common::uint8_t good = 0x02;
    while(good & 0x02)
        asm volatile("inb $0x64, %0" : "=a"(good));
    asm volatile("outb %0, $0x64" :: "a"((myos::common::uint8_t)0xFE));
    while(1) asm("hlt");
}