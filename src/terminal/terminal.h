#ifndef TERMINAL_H
#define TERMINAL_H

//Assign new values for non-default combinations. Numbers are so big to prevent code conflict. Like 127 can conflict with an ASCII character

//Arrow keys
#define KEY_UP        1001
#define KEY_DOWN      1002
#define KEY_RIGHT     1003
#define KEY_LEFT      1004

//Control + arrow
#define KEY_CTRL_RIGHT  1005
#define KEY_CTRL_LEFT   1006
#define KEY_CTRL_UP     1007
#define KEY_CTRL_DOWN   1008

// Shift + arrow
#define KEY_SHIFT_RIGHT  1009
#define KEY_SHIFT_LEFT   1010
#define KEY_SHIFT_UP     1011
#define KEY_SHIFT_DOWN   1012

//Ctrl+shift+arrow(selection)
#define KEY_CTRL_SHIFT_RIGHT  1013
#define KEY_CTRL_SHIFT_LEFT   1014

//Function keys
#define KEY_BACKSPACE   127
#define KEY_ENTER       13
#define KEY_ESC         27
#define KEY_TAB         9
#define KEY_DELETE      1020

//CTRL shortcuts
#define KEY_CTRL_C      3
#define KEY_CTRL_F      6
#define KEY_CTRL_G      7
#define KEY_CTRL_O      15
#define KEY_CTRL_R      18
#define KEY_CTRL_S      19
#define KEY_CTRL_V      22
#define KEY_CTRL_X      24
#define KEY_CTRL_Z      26
#define KEY_CTRL_W        23
#define KEY_ALT_BACKSPACE  300
#define KEY_CTRL_BACKSPACE 301
#define KEY_PASTE_START    2000
#define KEY_PASTE_END      2001
#define KEY_UTF8           2002


void term_enable_raw_mode();
void term_disable_raw_mode();

int term_read_key(char utf8_out[5]);

void term_show_cursor();
void term_get_size(int *rows,int *cols);
void term_reset_color();
void term_cursor_blink_on();
void term_cursor_blink_off();

#endif