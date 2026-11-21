#ifdef _WIN32

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

#include "terminal.h"
#include <stdio.h>
#include <windows.h>

//Terminal settings variables
static HANDLE hStdin;
static HANDLE hStdout;
static DWORD orig_in_mode;
static DWORD orig_out_mode;

//Terminal settings
void term_enable_raw_mode(){

    SetConsoleOutputCP(CP_UTF8);//Enable UTF-8 in terminal
    SetConsoleCP(CP_UTF8);

    hStdin=GetStdHandle(STD_INPUT_HANDLE);
    hStdout=GetStdHandle(STD_OUTPUT_HANDLE);

    GetConsoleMode(hStdin,&orig_in_mode); //Store old terminal settings to load again when finished
    GetConsoleMode(hStdout,&orig_out_mode);

    DWORD new_in=orig_in_mode;
    new_in&=~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT); //Disable echo(we render chars ourselves) , disable line buffer inputs, disable ctrl+c kill
    new_in|=ENABLE_WINDOW_INPUT | ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(hStdin,new_in);

    DWORD new_out=orig_out_mode;
    new_out|=ENABLE_VIRTUAL_TERMINAL_PROCESSING | ENABLE_PROCESSED_OUTPUT; //Enable ANSI escape code output processing
    SetConsoleMode(hStdout,new_out);
}

void term_disable_raw_mode(){
    SetConsoleMode(hStdin,orig_in_mode); //Load old settings
    SetConsoleMode(hStdout,orig_out_mode);
}

//INPUT READING
int term_read_key(char utf8_out[5]){
    if(utf8_out) utf8_out[0]='\0';

    INPUT_RECORD ir;
    DWORD read;

    for(;;){
        if(!ReadConsoleInputW(hStdin,&ir,1,&read)) return -1;
        if(read==0) continue;

        if(ir.EventType!=KEY_EVENT) continue;
        if(!ir.Event.KeyEvent.bKeyDown) continue;

        WORD vk=ir.Event.KeyEvent.wVirtualKeyCode;
        DWORD ctl=ir.Event.KeyEvent.dwControlKeyState;
        WCHAR wch=ir.Event.KeyEvent.uChar.UnicodeChar;

        //Are control keys pressed?
        int shift=(ctl & SHIFT_PRESSED)!=0;
        int ctrl=(ctl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))!=0;
        int alt=(ctl & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED))!=0;

        //CTRL combinations
        if(ctrl && !shift){
            switch(vk){
                case 'C': return KEY_CTRL_C;
                case 'F': return KEY_CTRL_F;
                case 'G': return KEY_CTRL_G;
                case 'O': return KEY_CTRL_O;
                case 'R': return KEY_CTRL_R;
                case 'S': return KEY_CTRL_S;
                case 'V': return KEY_CTRL_V;
                case 'W': return KEY_CTRL_W;
                case 'X': return KEY_CTRL_X;
                case 'Z': return KEY_CTRL_Z;
                case VK_BACK: return KEY_CTRL_BACKSPACE;
                default: break;
            }
        }

        //Arrow inputs with control keys and default
        switch(vk){
            case VK_UP:
                if(ctrl) return KEY_CTRL_UP;
                if(shift) return KEY_SHIFT_UP;
                return KEY_UP;

            case VK_DOWN:
                if(ctrl) return KEY_CTRL_DOWN;
                if(shift) return KEY_SHIFT_DOWN;
                return KEY_DOWN;

            case VK_RIGHT:
                if(ctrl && shift) return KEY_CTRL_SHIFT_RIGHT;
                if(ctrl) return KEY_CTRL_RIGHT;
                if(shift) return KEY_SHIFT_RIGHT;
                return KEY_RIGHT;

            case VK_LEFT:
                if(ctrl && shift) return KEY_CTRL_SHIFT_LEFT;
                if(ctrl) return KEY_CTRL_LEFT;
                if(shift) return KEY_SHIFT_LEFT;
                return KEY_LEFT;

            case VK_DELETE: return KEY_DELETE;
            case VK_BACK:
                if(alt) return KEY_ALT_BACKSPACE;
                if(ctrl) return KEY_CTRL_BACKSPACE;
                return KEY_BACKSPACE;
            case VK_RETURN: return KEY_ENTER;
            case VK_ESCAPE: return KEY_ESC;
            case VK_TAB: return KEY_TAB;
            default: break;
        }

        //Exclude modifiers
        if(vk==VK_SHIFT || vk==VK_CONTROL || vk==VK_MENU ||
            vk==VK_LSHIFT || vk==VK_RSHIFT ||
            vk==VK_LCONTROL || vk==VK_RCONTROL ||
            vk==VK_LMENU || vk==VK_RMENU)
            continue;

        //Normal inputs - widechar
        if(wch>=32 && wch!=127){

            if(wch<0x80){
                if(utf8_out){
                    utf8_out[0]=(char)wch;
                    utf8_out[1]='\0';
                }
                return (int)wch;
            }
            else{
                //Format UTF-16 to UTF8
                char mb[5]={0};
                WCHAR tmp=wch;
                int n=WideCharToMultiByte(CP_UTF8,0,&tmp,1,mb,4,NULL,NULL);
                if(n>=1){
                    mb[n]='\0';
                    if(utf8_out){
                        for(int i=0;i<=n;i++) utf8_out[i]=mb[i];
                    }
                    return KEY_UTF8;
                }
            }
        }
    }
}

//Terminal Visual Functions
void term_show_cursor(){
    printf("\033[?25h");
    fflush(stdout);
}

void term_get_size(int *rows,int *cols){
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    GetConsoleScreenBufferInfo(hStdout,&csbi);
    *cols=csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *rows=csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
}

void term_reset_color(){
    printf("\033[0m");
    fflush(stdout);
}

void term_cursor_blink_on(){
    printf("\033[?12h");
    printf("\033[6 q");
    fflush(stdout);
}

void term_cursor_blink_off(){
    printf("\033[?12l");
    fflush(stdout);
}

#endif