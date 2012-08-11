#include <stdint.h>

static struct {
    char c8;
    char16_t c16;
    char32_t c32;

    const char *s8;
    const char16_t *s16;
    const char32_t *s32;
} strings = {
    's', u't', U'p',
    // various encodings of "stapΑΩ☺😈"
    u8"stap\u0391\u03A9\u263A\U0001F608",
    u"stap\u0391\u03A9\u263A\U0001F608",
    U"stap\u0391\u03A9\u263A\U0001F608",
};

int main()
{
    return 0;
}
