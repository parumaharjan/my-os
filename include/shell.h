#ifndef __MYOS__SHELL_H
#define __MYOS__SHELL_H

#include <common/types.h>
#include <drivers/keyboard.h>
#include <scheduler.h>

#define BUF_SIZE 256

class Shell : public myos::drivers::KeyboardEventHandler
{
    char     buf[BUF_SIZE];
    int      bufPos;
    myos::Scheduler* sched;

    void printPrompt();
    void execute(char* cmd);

    void cmdHelp();
    void cmdClear();
    void cmdEcho(char* args);
    void cmdPs();
    void cmdThreads();
    void cmdKill(char* args);
    void cmdKillThread(char* args);
    void cmdVersion();
    void cmdMeminfo();
    void cmdPaging();
    void cmdReboot();

public:
    Shell(myos::Scheduler* s);
    void start();
    void OnKeyDown(char c) override;
};

#endif