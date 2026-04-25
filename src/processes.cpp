#include <processes.h>
#include <common/types.h>

// Each task writes its live tick counter into a fixed column
// on VGA row 24 (status bar) so round-robin is visible without
// corrupting shell output in rows 0-23.
//
// Row 24 layout:
//   col  0 : A: NNNNNN
//   col 27 : B: NNNNNN
//   col 54 : C: NNNNNN

static void writeStatusCell(int col, const char* label,
                             myos::common::uint32_t count,
                             myos::common::uint8_t color)
{
    myos::common::uint16_t* V = (myos::common::uint16_t*)0xb8000;
    int i = 0;
    for(; label[i]; i++)
        V[24*80 + col + i] = ((myos::common::uint16_t)color << 8)
                              | (myos::common::uint8_t)label[i];

    char buf[12]; int len = 0;
    myos::common::uint32_t n = count;
    if(n == 0) { buf[len++] = '0'; }
    else { while(n) { buf[len++] = '0' + (n % 10); n /= 10; } }

    for(int j = 0; j < len; j++)
        V[24*80 + col + i + j] = ((myos::common::uint16_t)color << 8)
                                  | (myos::common::uint8_t)buf[len-1-j];
    // pad to erase old longer numbers
    for(int j = len; j < 7; j++)
        V[24*80 + col + i + j] = ((myos::common::uint16_t)color << 8) | ' ';
}

void taskA_func()
{
    myos::common::uint32_t ticks = 0;
    while(1)
    {
        writeStatusCell(0, "A:", ticks++, 0x2F);
        for(volatile int d = 0; d < 500000; d++);
    }
}

void taskB_func()
{
    myos::common::uint32_t ticks = 0;
    while(1)
    {
        writeStatusCell(27, "B:", ticks++, 0x4F);
        for(volatile int d = 0; d < 500000; d++);
    }
}

void taskC_func()
{
    myos::common::uint32_t ticks = 0;
    while(1)
    {
        writeStatusCell(54, "C:", ticks++, 0x1F);
        for(volatile int d = 0; d < 500000; d++);
    }
}