#include <shell.h>
#include <vga.h>
#include <memorymanagement.h>

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
    const char* bar = "[ task-a: A ]          [ task-b: B ]"
                      "          [ task-c: C ]          ";
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

    if(streq(scratch, "help"))    { cmdHelp();     return; }
    if(streq(scratch, "clear"))   { cmdClear();    return; }
    if(streq(scratch, "echo"))    { cmdEcho(args); return; }
    if(streq(scratch, "ps"))      { cmdPs();       return; }
    if(streq(scratch, "kill"))    { cmdKill(args); return; }
    if(streq(scratch, "version")) { cmdVersion();  return; }
    if(streq(scratch, "meminfo")) { cmdMeminfo();  return; }
    if(streq(scratch, "reboot"))  { cmdReboot();   return; }

    printf("\nUnknown: ", 0x0C);
    printf(scratch, 0x0C);
    printf("  (type 'help')", 0x08);
}

void Shell::cmdHelp()
{
    printf("\n--- Commands ---\n", 0x0E);
    printf("  help       show this list\n",                    0x0F);
    printf("  clear      clear screen\n",                      0x0F);
    printf("  echo <x>   print text\n",                        0x0F);
    printf("  ps         list processes\n",                     0x0F);
    printf("  kill <pid> terminate a process\n",                0x0F);
    printf("  meminfo    heap details\n",                       0x0F);
    printf("  version    kernel info\n",                        0x0F);
    printf("  reboot     cold reboot\n",                        0x0F);
    printf("\n  Row 24 = live task tick counters\n",            0x08);
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
        if(sched->procs[i].active)
        {
            bool isCur = (i == sched->current);
            myos::common::uint8_t col = isCur ? 0x0A : 0x0F;

            printInt(sched->procs[i].pid);
            printf(isCur ? "    RUNNING   " : "    ready     ", col);
            printf(sched->procs[i].name, col);
            putChar('\n');
            any = true;
        }
    }
    if(!any) printf("(no processes)\n", 0x08);
}

void Shell::cmdKill(char* args)
{
    if(!args || !args[0]) { printf("\nusage: kill <pid>\n", 0x0C); return; }

    myos::common::uint32_t pid = 0;
    for(int i = 0; args[i] >= '0' && args[i] <= '9'; i++)
        pid = pid * 10 + (args[i] - '0');

    if(!sched) return;

    int slot = -1;
    for(int i = 0; i < MAX_PROCS; i++)
        if(sched->procs[i].active && sched->procs[i].pid == pid)
        { slot = i; break; }

    if(slot == -1)
    {
        printf("\nNo process with PID ", 0x0C);
        printInt(pid);
        putChar('\n');
        return;
    }

    char killedName[32];
    strcpy(killedName, sched->procs[slot].name);

    sched->killProcess(pid);

    // Clear that task's column on the status bar (row 24)
    myos::common::uint16_t* V = (myos::common::uint16_t*)0xb8000;
    int clearCol = -1;
    if(streq(killedName, "task-a")) clearCol = 0;
    else if(streq(killedName, "task-b")) clearCol = 27;
    else if(streq(killedName, "task-c")) clearCol = 54;

    if(clearCol >= 0)
    {
        const char* dead = "[terminated]     ";
        for(int i = 0; dead[i] && clearCol+i < 80; i++)
            V[24*80 + clearCol + i] = (0x08 << 8)
                                    | (myos::common::uint8_t)dead[i];
    }

    printf("\nKilled PID ", 0x0A);
    printInt(pid);
    printf(" (", 0x08);
    printf(killedName, 0x08);
    printf(")\n", 0x08);
}

void Shell::cmdVersion()
{
    printf("\nMyOS 0.1  |  kernel built from scratch\n", 0x0B);
    printf("Arch: x86 32-bit  |  Boot: GRUB/Multiboot\n", 0x08);
    printf("Features: GDT, IDT, PIC, keyboard,\n",        0x08);
    printf("          heap alloc, round-robin scheduler\n", 0x08);
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

void Shell::cmdReboot()
{
    printf("\nRebooting...\n", 0x0C);
    myos::common::uint8_t good = 0x02;
    while(good & 0x02)
        asm volatile("inb $0x64, %0" : "=a"(good));
    asm volatile("outb %0, $0x64" :: "a"((myos::common::uint8_t)0xFE));
    while(1) asm("hlt");
}