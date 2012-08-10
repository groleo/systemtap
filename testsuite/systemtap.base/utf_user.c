#include <stdint.h>

static uint16_t utf16_string[] = {
    0x73, 0x74, 0x61, 0x70, // "stap" (one-byte UTF-8 each)
    0x391, 0x3A9,           // "ΑΩ" (two-byte UTF-8 each)
    0x263A,                 // "☺" (three-byte UTF-8)
    0xD83D, 0xDE08,         // U+1F608 "😈" (four-byte UTF-8)
    0
};

static uint32_t utf32_string[] = {
    0x73, 0x74, 0x61, 0x70, // "stap" (one-byte UTF-8 each)
    0x391, 0x3A9,           // "ΑΩ" (two-byte UTF-8 each)
    0x263A,                 // "☺" (three-byte UTF-8)
    0x1F608,                // "😈" (four-byte UTF-8)
    0
};

int main()
{
    return 0;
}
