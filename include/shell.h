#ifndef __MYOS__SHELL_H
#define __MYOS__SHELL_H

#include <common/types.h>
#include <drivers/keyboard.h>

namespace myos {

// ── forward declarations ────────────────────────────────────────────────────
struct  MemoryChunk;
class   MemoryManager;
struct  Process;
class   Scheduler;

// ── Shell ───────────────────────────────────────────────────────────────────
#define SHELL_BUF_SIZE 256

class Shell : public drivers::KeyboardEventHandler
{
    char       inputBuffer[SHELL_BUF_SIZE];
    int        bufPos;
    Scheduler* scheduler;

    // internal helpers
    void printPrompt();
    void execute(char* cmd);

    // commands
    void cmdHelp();
    void cmdClear();
    void cmdEcho(char* args);
    void cmdPs();
    void cmdKill(char* args);
    void cmdMeminfo();
    void cmdMemtest();
    void cmdVersion();
    void cmdReboot();

public:
    Shell(Scheduler* sched);
    void Run();                          // call once after kernel init
    void OnKeyDown(char c) override;
};

} // namespace myos

#endif