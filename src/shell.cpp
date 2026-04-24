// src/shell.cpp
#include "shell.h"

// Minimal VGA text output
static uint16_t* vga = (uint16_t*)0xB8000;
static int cursorX = 0, cursorY = 0;
static const int VGA_WIDTH = 80, VGA_HEIGHT = 25;

static void vga_putchar(char c, uint8_t color = 0x07) {
    if (c == '\n') { cursorX = 0; cursorY++; }
    else if (c == '\b') {
        if (cursorX > 0) { cursorX--; vga[cursorY * VGA_WIDTH + cursorX] = ' ' | (color << 8); }
    } else {
        vga[cursorY * VGA_WIDTH + cursorX] = (uint8_t)c | (color << 8);
        cursorX++;
        if (cursorX >= VGA_WIDTH) { cursorX = 0; cursorY++; }
    }
    // Scroll
    if (cursorY >= VGA_HEIGHT) {
        for (int y = 0; y < VGA_HEIGHT - 1; y++)
            for (int x = 0; x < VGA_WIDTH; x++)
                vga[y * VGA_WIDTH + x] = vga[(y+1) * VGA_WIDTH + x];
        for (int x = 0; x < VGA_WIDTH; x++)
            vga[(VGA_HEIGHT-1) * VGA_WIDTH + x] = ' ' | (0x07 << 8);
        cursorY = VGA_HEIGHT - 1;
    }
}

Shell::Shell(Scheduler* sched) : bufPos(0), scheduler(sched) {
    for (int i = 0; i < SHELL_BUF_SIZE; i++) inputBuffer[i] = 0;
}

void Shell::PrintStr(const char* s) {
    for (int i = 0; s[i]; i++) vga_putchar(s[i]);
}

void Shell::PrintPrompt() {
    PrintStr("\nmyos> ");
}

void Shell::Run() {
    // Clear screen
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) vga[i] = ' ' | (0x07 << 8);
    cursorX = 0; cursorY = 0;
    PrintStr("MyOS Shell v0.1 - type 'help'\n");
    PrintPrompt();
}

void Shell::OnKeyDown(char c) {
    if (c == '\n') {
        vga_putchar('\n');
        inputBuffer[bufPos] = 0;
        if (bufPos > 0) Execute(inputBuffer);
        bufPos = 0;
        PrintPrompt();
    } else if (c == '\b') {
        if (bufPos > 0) { bufPos--; vga_putchar('\b'); }
    } else if (bufPos < SHELL_BUF_SIZE - 1) {
        inputBuffer[bufPos++] = c;
        vga_putchar(c);
    }
}

// Simple string comparison
static bool streq(const char* a, const char* b) {
    for (int i = 0; ; i++) {
        if (a[i] != b[i]) return false;
        if (!a[i]) return true;
    }
}

// Find first space and return pointer past it (args)
static char* getargs(char* cmd) {
    int i = 0;
    while (cmd[i] && cmd[i] != ' ') i++;
    if (!cmd[i]) return cmd + i;
    cmd[i] = 0;
    return cmd + i + 1;
}

void Shell::CmdHelp() {
    PrintStr("\nAvailable commands:\n");
    PrintStr("  help      - show this message\n");
    PrintStr("  clear     - clear the screen\n");
    PrintStr("  echo <x>  - print text\n");
    PrintStr("  ps        - list processes\n");
    PrintStr("  version   - kernel version\n");
}

void Shell::CmdClear() {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) vga[i] = ' ' | (0x07 << 8);
    cursorX = 0; cursorY = 0;
}

void Shell::CmdPs() {
    if (!scheduler) { PrintStr("\nNo scheduler.\n"); return; }
    PrintStr("\nPID  NAME\n");
    // Print active processes — access scheduler internals via friend or public API
    PrintStr("(Add scheduler->ListProcesses() method)\n");
}

void Shell::CmdEcho(char* args) {
    vga_putchar('\n');
    for (int i = 0; args[i]; i++) vga_putchar(args[i]);
}

void Shell::Execute(char* cmd) {
    char* args = getargs(cmd);
    if (streq(cmd, "help"))    { CmdHelp(); return; }
    if (streq(cmd, "clear"))   { CmdClear(); return; }
    if (streq(cmd, "echo"))    { CmdEcho(args); return; }
    if (streq(cmd, "ps"))      { CmdPs(); return; }
    if (streq(cmd, "version")) { PrintStr("\nMyOS 0.1 - built from scratch\n"); return; }
    PrintStr("\nUnknown command: ");
    PrintStr(cmd);
    PrintStr("\n");
}