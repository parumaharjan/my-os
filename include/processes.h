#ifndef __MYOS__PROCESSES_H
#define __MYOS__PROCESSES_H

void taskA_func();
void taskB_func();
void taskC_func();
void threadA_func();
void threadB_func();

extern void* threadA_mem;
extern void* threadB_mem;

#endif