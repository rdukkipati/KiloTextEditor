#include <stdint.h>

#define internal        static
#define local_persist   static
#define global_variable static

typedef int8_t  i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;
typedef i32     b32;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

global_variable termios ORIGINAL_TERMIOS;

internal void
DisableRawMode()
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &ORIGINAL_TERMIOS);
}

internal void
EnableRawMode()
{
    tcgetattr(STDIN_FILENO, &ORIGINAL_TERMIOS);
    atexit(DisableRawMode);
    termios Termios = ORIGINAL_TERMIOS;
    Termios.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &Termios);
}

i32
main()
{
    EnableRawMode();
    char c;
    while(read(STDIN_FILENO, &c, 1) == 1 && c!= 'q')
    {
        if(iscntrl(c))
        {
            printf("%d\n", c);
        }
        else
        {
            printf("%d ('%c')\n", c, c);
        }
    }
    return 0;
}
