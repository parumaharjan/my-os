#ifndef __MYOS__VGA_H
#define __MYOS__VGA_H

#include <common/types.h>

void serialWrite(char c);
void clearScreen();
void scrollUp();
void putChar(char c, myos::common::uint8_t color = 0x0F);
void printf(const char* str, myos::common::uint8_t color = 0x0F);
void printInt(myos::common::uint32_t n);
void printfHex(myos::common::uint8_t key);

#endif