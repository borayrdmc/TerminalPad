#ifndef _WIN32

#include "terminal.h"
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <locale.h>

static struct termios orig_termios;
static int raw_mode_active=0;

static void writeAll(const char *data,size_t len){
    ssize_t written=write(STDOUT_FILENO,data,len);
    (void)written;
}

static int readByteTimeout(unsigned char *out_byte){
    struct termios temp_t;
    tcgetattr(STDIN_FILENO,&temp_t);

    struct termios timeout_t=temp_t;
    timeout_t.c_cc[VMIN]=0;
    timeout_t.c_cc[VTIME]=1; //100ms timeout to separate standalone ESC from escape sequences
    tcsetattr(STDIN_FILENO,TCSANOW,&timeout_t);

    ssize_t n=read(STDIN_FILENO,out_byte,1);

    tcsetattr(STDIN_FILENO,TCSANOW,&temp_t);
    return n==1;
}

//Terminal settings
void term_enable_raw_mode(){
    setlocale(LC_ALL,"");

    tcgetattr(STDIN_FILENO,&orig_termios);
    struct termios raw=orig_termios;

    raw.c_iflag&=~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_cflag|=(CS8);
    raw.c_lflag&=~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN]=1;
    raw.c_cc[VTIME]=0;

    tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw);
    raw_mode_active=1;

    //Enable bracketed paste mode (ESC[200~ ... ESC[201~)
    writeAll("\033[?2004h",8);
}

void term_disable_raw_mode(){
    if(raw_mode_active){
        writeAll("\033[?2004l",8);
        tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_termios);
        raw_mode_active=0;
    }
}

//Input reading
int term_read_key(char utf8_out[5]){
    if(utf8_out!=NULL) utf8_out[0]='\0';

    unsigned char c;
    if(read(STDIN_FILENO,&c,1)!=1) return -1;

    //CTRL shortcuts
    if(c==3) return KEY_CTRL_C;
    if(c==6) return KEY_CTRL_F;
    if(c==7) return KEY_CTRL_G;
    if(c==15) return KEY_CTRL_O;
    if(c==18) return KEY_CTRL_R;
    if(c==19) return KEY_CTRL_S;
    if(c==22) return KEY_CTRL_V;
    if(c==23) return KEY_CTRL_W;
    if(c==24) return KEY_CTRL_X;
    if(c==26) return KEY_CTRL_Z;

    //Escape sequences (arrows, modifiers, Delete, bracketed paste)
    if(c==27){
        unsigned char seq0=0;
        if(!readByteTimeout(&seq0)) return KEY_ESC;

        //Alt+Backspace
        if(seq0==0x7F || seq0==0x08) return KEY_ALT_BACKSPACE;

        unsigned char seq1=0;
        if(!readByteTimeout(&seq1)) return KEY_ESC;

        if(seq0=='['){
            switch(seq1){
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'c': return KEY_SHIFT_RIGHT;
                case 'd': return KEY_SHIFT_LEFT;
            }

            if(seq1>='0' && seq1<='9'){
                unsigned char seq2=0;
                if(!readByteTimeout(&seq2)) return KEY_ESC;

                if(seq2=='C'){
                    if(seq1=='5') return KEY_CTRL_RIGHT;
                    if(seq1=='6') return KEY_CTRL_SHIFT_RIGHT;
                    if(seq1=='2') return KEY_SHIFT_RIGHT;
                }
                if(seq2=='D'){
                    if(seq1=='5') return KEY_CTRL_LEFT;
                    if(seq1=='6') return KEY_CTRL_SHIFT_LEFT;
                    if(seq1=='2') return KEY_SHIFT_LEFT;
                }
                if(seq2=='A'){
                    if(seq1=='5') return KEY_CTRL_UP;
                    if(seq1=='2') return KEY_SHIFT_UP;
                }
                if(seq2=='B'){
                    if(seq1=='5') return KEY_CTRL_DOWN;
                    if(seq1=='2') return KEY_SHIFT_DOWN;
                }

                if(seq1=='3' && seq2=='~') return KEY_DELETE;

                //Bracketed paste: ESC[200~ / ESC[201~
                if(seq1=='2' && seq2=='0'){
                    unsigned char seq3=0;
                    unsigned char seq4=0;
                    if(!readByteTimeout(&seq3) || !readByteTimeout(&seq4)) return KEY_ESC;
                    if(seq4=='~'){
                        if(seq3=='0') return KEY_PASTE_START;
                        if(seq3=='1') return KEY_PASTE_END;
                    }
                }

                //Xterm modifier format: ESC[1;<mod><dir>
                if(seq1=='1' && seq2==';'){
                    unsigned char mod=0;
                    unsigned char dir=0;
                    if(!readByteTimeout(&mod) || !readByteTimeout(&dir)) return KEY_ESC;

                    if(mod=='5'){
                        if(dir=='A') return KEY_CTRL_UP;
                        if(dir=='B') return KEY_CTRL_DOWN;
                        if(dir=='C') return KEY_CTRL_RIGHT;
                        if(dir=='D') return KEY_CTRL_LEFT;
                    }
                    if(mod=='2'){
                        if(dir=='A') return KEY_SHIFT_UP;
                        if(dir=='B') return KEY_SHIFT_DOWN;
                        if(dir=='C') return KEY_SHIFT_RIGHT;
                        if(dir=='D') return KEY_SHIFT_LEFT;
                    }
                    if(mod=='6'){
                        if(dir=='C') return KEY_CTRL_SHIFT_RIGHT;
                        if(dir=='D') return KEY_CTRL_SHIFT_LEFT;
                    }
                }
            }
            return KEY_ESC;
        }else if(seq0=='O'){
            switch(seq1){
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'C': return KEY_RIGHT;
                case 'D': return KEY_LEFT;
                case 'a': return KEY_CTRL_UP;
                case 'b': return KEY_CTRL_DOWN;
                case 'c': return KEY_CTRL_RIGHT;
                case 'd': return KEY_CTRL_LEFT;
            }
            if(seq1>='0' && seq1<='9'){
                unsigned char seq2=0;
                if(!readByteTimeout(&seq2)) return KEY_ESC;

                if(seq2=='c' || seq2=='C'){
                    if(seq1=='5') return KEY_CTRL_RIGHT;
                    if(seq1=='6') return KEY_CTRL_SHIFT_RIGHT;
                    if(seq1=='2') return KEY_SHIFT_RIGHT;
                }
                if(seq2=='d' || seq2=='D'){
                    if(seq1=='5') return KEY_CTRL_LEFT;
                    if(seq1=='6') return KEY_CTRL_SHIFT_LEFT;
                    if(seq1=='2') return KEY_SHIFT_LEFT;
                }
            }
            return KEY_ESC;
        }

        return KEY_ESC;
    }

    if(c==127 || c==8) return KEY_BACKSPACE;

    //Multi-byte UTF-8 character
    if(c>=0x80){
        int len=1;
        if((c & 0xE0)==0xC0) len=2;
        else if((c & 0xF0)==0xE0) len=3;
        else if((c & 0xF8)==0xF0) len=4;

        char mb[5]={0};
        mb[0]=(char)c;
        for(int i=1;i<len;i++){
            unsigned char next_b=0;
            if(read(STDIN_FILENO,&next_b,1)!=1) break;
            mb[i]=(char)next_b;
        }
        if(utf8_out!=NULL){
            for(int i=0;i<=len;i++) utf8_out[i]=mb[i];
        }
        return KEY_UTF8;
    }

    if(c>=32 && c<127 && utf8_out!=NULL){
        utf8_out[0]=(char)c;
        utf8_out[1]='\0';
    }
    return (int)c;
}

//Terminal visual functions
void term_show_cursor(){
    writeAll("\033[?25h",6);
}

void term_get_size(int *rows,int *cols){
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)==-1 || ws.ws_col==0 || ws.ws_row==0){
        *rows=24;
        *cols=80;
        return;
    }
    *rows=ws.ws_row;
    *cols=ws.ws_col;
}

void term_reset_color(){
    writeAll("\033[0m",4);
}

void term_cursor_blink_on(){
    writeAll("\033[5 q",5);
}

void term_cursor_blink_off(){
    writeAll("\033[?12l",6);
}

#endif
