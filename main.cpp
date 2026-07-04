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
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#define CTRL_KEY(k) ((k) & 0x1f)

struct editor_config
{
    int ScreenRows;
    int ScreenCols;
    termios OriginalTermios;
};

global_variable editor_config EDITOR_CONFIG;

internal void
Die(const char *ErrorMessage)
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    perror(ErrorMessage);
    exit(1);
}

internal void
DisableRawMode()
{
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &EDITOR_CONFIG.OriginalTermios) == -1)
    {
        Die("tcsetattr");
    }
}

internal void
EnableRawMode()
{
    if(tcgetattr(STDIN_FILENO, &EDITOR_CONFIG.OriginalTermios) == -1)
    {
        Die("tcgetattr");
    }
    atexit(DisableRawMode);
    termios Termios = EDITOR_CONFIG.OriginalTermios;
    Termios.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    Termios.c_oflag &= ~(OPOST);
    Termios.c_cflag |= (CS8);
    Termios.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    Termios.c_cc[VMIN] = 0;
    Termios.c_cc[VTIME] = 1;
    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &Termios) == -1)
    {
        Die("tcsetattr");
    }
}

internal char
EditorReadKey()
{
    int BytesRead;
    char Char;
    while((BytesRead = read(STDIN_FILENO, &Char, 1)) != 1)
    {
        if(BytesRead == -1 && errno != EAGAIN)
        {
            Die("read");
        }
    }
    return Char;
}

internal void
EditorProcessKeypress()
{
    char Char = EditorReadKey();
    switch(Char)
    {
        case CTRL_KEY('q'):
        {
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            
        } break;
    }
}

internal void
EditorDrawRows()
{
    for(int Row = 0; Row < EDITOR_CONFIG.ScreenRows; ++Row)
    {
        write(STDOUT_FILENO, "~\r\n", 3);
    }
}

internal void
EditorRefreshScreen()
{
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    
    EditorDrawRows();
    
    write(STDOUT_FILENO, "\x1b[H", 3);
}

internal int
GetWindowSize(int *Rows, int *Cols)
{
    winsize WinSize;
    if(1 || ioctl(STDOUT_FILENO, TIOCGWINSZ, &WinSize) == -1 || WinSize.ws_col == 0)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        {
            return -1;
        }
        EditorReadKey();
        return -1;
    }
    else
    {
        *Cols = WinSize.ws_col;
        *Rows = WinSize.ws_row;
        return 0;
    }
}

internal void
InitEditor()
{
    if(GetWindowSize(&EDITOR_CONFIG.ScreenRows, &EDITOR_CONFIG.ScreenCols) == -1)
    {
        Die("GetWindowSize");
    }
}

i32
main()
{
    EnableRawMode();
    InitEditor();
    while(1)
    {
        EditorRefreshScreen();
        EditorProcessKeypress();
    }
    
    return 0;
}
