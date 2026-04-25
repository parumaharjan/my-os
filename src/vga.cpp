#include <vga.h>
#include <hardwarecommunication/port.h>

using namespace myos::hardwarecommunication;
using namespace myos::common;

// ===================== SERIAL PORT (0x3F8 = COM1) =====================

static void serialInit()
{
    Port8Bit port3F8(0x3F8);
    Port8Bit port3F9(0x3F9);
    Port8Bit port3FA(0x3FA);
    Port8Bit port3FB(0x3FB);
    Port8Bit port3FC(0x3FC);

    port3F9.Write(0x00); // disable interrupts
    port3FB.Write(0x80); // enable DLAB (set baud rate divisor)
    port3F8.Write(0x03); // baud divisor low byte (38400 baud)
    port3F9.Write(0x00); // baud divisor high byte
    port3FB.Write(0x03); // 8 bits, no parity, one stop bit
    port3FA.Write(0xC7); // enable FIFO
    port3FC.Write(0x0B); // enable RTS/DSR
}

void serialWrite(char c)
{
    Port8Bit port3FD(0x3FD);
    Port8Bit port3F8(0x3F8);

    // wait until transmit buffer is empty
    int timeout = 10000;
    while(!(port3FD.Read() & 0x20) && timeout-- > 0);
    port3F8.Write((uint8_t)c);
}

static void serialPrint(const char* str)
{
    for(int i = 0; str[i]; i++) serialWrite(str[i]);
}


// ===================== VGA =====================

static uint16_t* VideoMemory = (uint16_t*)0xb8000;
static uint8_t cursorX = 0, cursorY = 0;
static bool serialReady = false;

void clearScreen()
{
    for(int i = 0; i < 80*24; i++)
        VideoMemory[i] = (0x0F << 8) | ' ';
    cursorX = 0;
    cursorY = 0;

    // Init serial on first clearScreen call
    if(!serialReady)
    {
        serialInit();
        serialReady = true;
        serialPrint("\r\n=== MyOS Serial Log ===\r\n");
    }
}

void scrollUp()
{
    // Only scroll rows 0-21, rows 22-24 are status rows
    for(int y = 0; y < 21; y++)
        for(int x = 0; x < 80; x++)
            VideoMemory[80*y + x] = VideoMemory[80*(y+1) + x];
    for(int x = 0; x < 80; x++)
        VideoMemory[80*21 + x] = (0x0F << 8) | ' ';
    cursorY = 21;
}

void putChar(char c, uint8_t color)
{
    // Mirror to serial
    if(serialReady)
    {
        if(c == '\b')
            serialWrite('\b');
        else
            serialWrite(c);
    }

    if(c == '\n')
    {
        cursorX = 0;
        cursorY++;
        if(serialReady) serialWrite('\r'); // serial needs \r\n
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

    if(cursorY >= 22)  // stop at row 21, rows 22-24 reserved
        scrollUp();
}

void printf(const char* str, uint8_t color)
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