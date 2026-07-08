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
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

#define KILO_VERSION "0.0.1"
#define KILO_TAB_STOP 8

#define CTRL_KEY(k) ((k) & 0x1f)

struct editor_row
{
    int Size;
    int RenderSize;
    char *Chars;
    char *Render;
};

enum editor_key
{
    ARROW_LEFT = 1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};

struct editor_config
{
    int CursorX;
    int CursorY;
    int RenderX;
    int RowOffset;
    int ColOffset;
    int ScreenRows;
    int ScreenCols;
    int NumRows;
    editor_row *EditorRow;
    termios OriginalTermios;
};

global_variable editor_config EDITOR_CONFIG;

struct dynamic_string
{
    char *Buffer;
    int Length;
};

#define DYNAMIC_STRING_INIT {NULL, 0}

internal void
DynamicString_Append(dynamic_string *String, const char *Addition, int AdditionalLength)
{
    char *NewBuffer = (char *)realloc(String->Buffer, String->Length + AdditionalLength);
    if(NewBuffer == NULL)
    {
        return;
    }
    memcpy(&NewBuffer[String->Length], Addition, AdditionalLength);
    String->Buffer = NewBuffer;
    String->Length += AdditionalLength;
}

internal void
DynamicString_Free(dynamic_string *String)
{
    free(String->Buffer);
}

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

internal int
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
    if(Char  == '\x1b')
    {
        char Seq[3];
        if(read(STDIN_FILENO, &Seq[0], 1) != 1)
        {
            return '\x1b';
        }
        if(read(STDIN_FILENO, &Seq[1], 1) != 1)
        {
            return '\x1b';
        }
        if(Seq[0] == '[')
        {
            if(Seq[1] >= '0' && Seq[1] <= '9')
            {
                if(read(STDIN_FILENO, &Seq[2], 1) != 1)
                {
                    return '\x1b';
                }
                if(Seq[2] == '~')
                {
                    switch(Seq[1])
                    {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5': return PAGE_UP;
                        case '6': return PAGE_DOWN;
                        case '7': return HOME_KEY;
                        case '8': return END_KEY;
                    }
                }
            }
            else
            {
                switch (Seq[1])
                {
                    case 'A': return ARROW_UP;
                    case 'B': return ARROW_DOWN;
                    case 'C': return ARROW_RIGHT;
                    case 'D': return ARROW_LEFT;
                    case 'H': return HOME_KEY;
                    case 'F': return END_KEY;
                }
            }


        }
        else if(Seq[0] == 'O')
        {
            switch(Seq[1])
            {
                case 'H': return HOME_KEY;
                case 'F': return END_KEY;
            }
        }
        return '\x1b';
        
    }
    else
    {
        return Char;
    }

}



internal void
EditorMoveCursor(int Key)
{
    int *CursorX = &EDITOR_CONFIG.CursorX;
    int *CursorY = &EDITOR_CONFIG.CursorY;
    editor_row *EditorRow = (EDITOR_CONFIG.CursorY >= EDITOR_CONFIG.NumRows) ? NULL : &EDITOR_CONFIG.EditorRow[EDITOR_CONFIG.CursorY];
    switch(Key)
    {
        case ARROW_LEFT:
        {
            if(*CursorX != 0)
            {
                --(*CursorX);
            }
            else if(*CursorY > 0)
            {
                --(*CursorY);
                *CursorX = EDITOR_CONFIG.EditorRow[*CursorY].Size;
            }
            
        } break;
        case ARROW_RIGHT:
        {
            if(EditorRow && *CursorX < EditorRow->Size)
            {
                ++(*CursorX);
            }
            else if(EditorRow && *CursorX == EditorRow->Size)
            {
                ++(*CursorY);
                *CursorX = 0;
            }
            
        } break;
        case ARROW_UP:
        {
            if(*CursorY != 0)
            {
                --(*CursorY);
            }
            
        } break;
        case ARROW_DOWN:
        {
            if(*CursorY < EDITOR_CONFIG.NumRows)
            {
                ++(*CursorY);
            }
            
        } break;
    }
    
    EditorRow = (*CursorY >= EDITOR_CONFIG.NumRows) ? NULL : &EDITOR_CONFIG.EditorRow[*CursorY];
    int RowLength = EditorRow ? EditorRow->Size : 0;
    if(*CursorX > RowLength)
    {
        *CursorX = RowLength;
    }
}

internal void
EditorProcessKeypress()
{
    int Char = EditorReadKey();
    switch(Char)
    {
        case CTRL_KEY('q'):
        {
            write(STDOUT_FILENO, "\x1b[2J", 4);
            write(STDOUT_FILENO, "\x1b[H", 3);
            exit(0);
            
        } break;
        
        case HOME_KEY:
            EDITOR_CONFIG.CursorX = 0;
            break;
            
        case END_KEY:
            EDITOR_CONFIG.CursorX = EDITOR_CONFIG.ScreenRows - 1;
            break;
            
        case PAGE_UP:
        case PAGE_DOWN:
        {
            int Times = EDITOR_CONFIG.ScreenRows;
            while(--Times)
            {
                EditorMoveCursor(Char == PAGE_UP ? ARROW_UP : ARROW_DOWN);
            }
            
        } break;
            
        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
        {
            EditorMoveCursor(Char);
            
        } break;
    }
}

internal void
EditorDrawRows(dynamic_string *String)
{
    for(int Row = 0; Row < EDITOR_CONFIG.ScreenRows; ++Row)
    {
        int FileRow  = Row + EDITOR_CONFIG.RowOffset;
        if(FileRow >= EDITOR_CONFIG.NumRows)
        {
            if(EDITOR_CONFIG.NumRows == 0 && Row == EDITOR_CONFIG.ScreenRows / 3)
            {
                char Welcome[80];
                int WelcomeLength = snprintf(Welcome, sizeof(Welcome), "Kilo editor -- version %s", KILO_VERSION);
                if(WelcomeLength > EDITOR_CONFIG.ScreenCols)
                {
                    WelcomeLength = EDITOR_CONFIG.ScreenCols;
                }
                int Padding = (EDITOR_CONFIG.ScreenCols - WelcomeLength) / 2;
                if(Padding)
                {
                    DynamicString_Append(String, "~", 1);
                    --Padding;
                }
                while(Padding--)
                {
                    DynamicString_Append(String, " ", 1);
                }
                DynamicString_Append(String, Welcome, WelcomeLength);
            }
            else
            {
                DynamicString_Append(String, "~", 1);
            }
        }
        else
        {
            int Length = EDITOR_CONFIG.EditorRow[FileRow].RenderSize - EDITOR_CONFIG.ColOffset;
            if(Length < 0)
            {
                Length = 0;
            }
            if(Length > EDITOR_CONFIG.ScreenCols)
            {
                Length = EDITOR_CONFIG.ScreenCols;
            }
            DynamicString_Append(String, &EDITOR_CONFIG.EditorRow[FileRow].Render[EDITOR_CONFIG.ColOffset], Length);
        }

        DynamicString_Append(String, "\x1b[K", 3);
        if(Row < EDITOR_CONFIG.ScreenRows - 1)
        {
            DynamicString_Append(String, "\r\n", 2);
        }
    }
}

internal int
EditorRowCxToRx(editor_row *EditorRow, int CursorX)
{
    int rx = 0;
    int j;
    for(j = 0; j < CursorX; ++j)
    {
        if(EditorRow->Chars[j] == '\t')
        {
            rx += (KILO_TAB_STOP - 1) - (rx % KILO_TAB_STOP);
        }
        ++rx;
    }
    return rx;
}

internal void
EditorScroll()
{
    EDITOR_CONFIG.RenderX = 0;
    if(EDITOR_CONFIG.CursorY < EDITOR_CONFIG.NumRows)
    {
        EDITOR_CONFIG.RenderX = EditorRowCxToRx(&EDITOR_CONFIG.EditorRow[EDITOR_CONFIG.CursorY], EDITOR_CONFIG.CursorX);
    }
    if(EDITOR_CONFIG.CursorY < EDITOR_CONFIG.RowOffset)
    {
        EDITOR_CONFIG.RowOffset = EDITOR_CONFIG.CursorY;
    }
    if(EDITOR_CONFIG.CursorY >= EDITOR_CONFIG.RowOffset + EDITOR_CONFIG.ScreenRows)
    {
        EDITOR_CONFIG.RowOffset = EDITOR_CONFIG.CursorY - EDITOR_CONFIG.ScreenRows + 1;
    }
    if(EDITOR_CONFIG.RenderX < EDITOR_CONFIG.ColOffset)
    {
        EDITOR_CONFIG.ColOffset = EDITOR_CONFIG.RenderX;
    }
    if(EDITOR_CONFIG.RenderX >= EDITOR_CONFIG.ColOffset + EDITOR_CONFIG.ScreenCols)
    {
        EDITOR_CONFIG.ColOffset = EDITOR_CONFIG.RenderX - EDITOR_CONFIG.ScreenCols + 1;
    }
}

internal void
EditorRefreshScreen()
{
    EditorScroll();
    
    dynamic_string String = DYNAMIC_STRING_INIT;
    
    DynamicString_Append(&String, "\x1b[?25l", 6);
    DynamicString_Append(&String, "\x1b[H", 3);
    
    EditorDrawRows(&String);
    
    char Buffer[32];
    snprintf(Buffer, sizeof(Buffer), "\x1b[%d;%dH", EDITOR_CONFIG.CursorY - EDITOR_CONFIG.RowOffset + 1, EDITOR_CONFIG.RenderX - EDITOR_CONFIG.ColOffset + 1);
    DynamicString_Append(&String, Buffer, strlen(Buffer));

    DynamicString_Append(&String, "\x1b[?25h", 6);
    
    write(STDOUT_FILENO, String.Buffer, String.Length);
    DynamicString_Free(&String);
}

internal int
GetCursorPosition(int *Rows, int *Cols)
{
    char Buffer[32];
    u32 Index = 0;
    if(write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    {
        return -1;
    }
    while(Index < sizeof(Buffer) - 1)
    {
        if(read(STDIN_FILENO, &Buffer[Index], 1) != 1)
        {
            break;
        }
        if(Buffer[Index] == 'R')
        {
            break;
        }
        ++Index;
    }
    Buffer[Index] = '\0';
    if(Buffer[0] != '\x1b' || Buffer[1] != '[')
    {
        return -1;
    }
    if(sscanf(&Buffer[2], "%d;%d", Rows, Cols) != 2)
    {
        return -1;
    }
    
    return 0;
}

internal int
GetWindowSize(int *Rows, int *Cols)
{
    winsize WinSize;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &WinSize) == -1 || WinSize.ws_col == 0)
    {
        if(write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
        {
            return -1;
        }
        return GetCursorPosition(Rows, Cols);
    }
    else
    {
        *Cols = WinSize.ws_col;
        *Rows = WinSize.ws_row;
        return 0;
    }
}

internal void
EditorUpdateRow(editor_row *EditorRow)
{
    int tabs = 0;
    int j;
    for(j = 0; j < EditorRow->Size; ++j)
    {
        if(EditorRow->Chars[j] == '\t')
        {
            ++tabs;
        }
    }
    free(EditorRow->Render);
    EditorRow->Render = (char *)malloc(EditorRow->Size + tabs*(KILO_TAB_STOP - 1) + 1);
    
    int idx = 0;
    for(j = 0; j < EditorRow->Size; ++j)
    {
        if(EditorRow->Chars[j] == '\t')
        {
            EditorRow->Render[idx++] = ' ';
            while(idx % KILO_TAB_STOP != 0)
            {
                EditorRow->Render[idx++] = ' ';
            }
        }
        else
        {
            EditorRow->Render[idx++] = EditorRow->Chars[j];
        }
    }
    EditorRow->Render[idx] = '\0';
    EditorRow->RenderSize = idx;
}

internal void
EditorAppendRow(char *Line, size_t LineLength)
{
    editor_row **EditorRow = &EDITOR_CONFIG.EditorRow;
    *EditorRow = (editor_row *)realloc(*EditorRow, sizeof(editor_row) * (EDITOR_CONFIG.NumRows + 1));
    int Index = EDITOR_CONFIG.NumRows;
    (*EditorRow)[Index].Size = LineLength;
    (*EditorRow)[Index].Chars = (char *)malloc(LineLength + 1);
    memcpy((*EditorRow)[Index].Chars, Line, LineLength);
    (*EditorRow)[Index].Chars[LineLength] = '\0';
    (*EditorRow)[Index].RenderSize = 0;
    (*EditorRow)[Index].Render = NULL;
    EditorUpdateRow(&(*EditorRow)[Index]);
    ++EDITOR_CONFIG.NumRows;
}

internal void
EditorOpen(char *Filename)
{
    FILE *File = fopen(Filename, "r");
    if(!File)
    {
        Die("fopen");
    }
    
    char *Line  = NULL;
    size_t LineCap = 0;
    ssize_t LineLength;
    while((LineLength = getline(&Line, &LineCap, File)) != -1)
    {
        while(LineLength > 0 && (Line[LineLength - 1] == '\n' || Line[LineLength - 1] == 'r'))
        {
            --LineLength;
        }
        EditorAppendRow(Line, LineLength);
    }
    free(Line);
    fclose(File);
    

}

internal void
InitEditor()
{
    EDITOR_CONFIG.CursorX = 0;
    EDITOR_CONFIG.CursorY = 0;
    EDITOR_CONFIG.RenderX = 0;
    EDITOR_CONFIG.RowOffset = 0;
    EDITOR_CONFIG.ColOffset = 0;
    EDITOR_CONFIG.NumRows = 0;
    EDITOR_CONFIG.EditorRow = NULL;
    if(GetWindowSize(&EDITOR_CONFIG.ScreenRows, &EDITOR_CONFIG.ScreenCols) == -1)
    {
        Die("GetWindowSize");
    }
}

int
main(int argc, char *argv[])
{
    EnableRawMode();
    InitEditor();
    if(argc >= 2)
    {
        EditorOpen(argv[1]);
    }
    while(1)
    {
        EditorRefreshScreen();
        EditorProcessKeypress();
    }
    
    return 0;
}
