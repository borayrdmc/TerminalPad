#ifndef RENDERER_H
#define RENDERER_H

#include "../text-engine/buffer/buffer.h"
#include "../text-engine/cursor/cursor.h"

extern int save_active;
extern int input_prompt_active;
extern char save_filename_buf[256];
extern int browser_mode;
extern int exit_prompt_active;

void renderer_draw(int terminal_rows,int terminal_cols);
void renderer_draw_filebrowser(int terminal_rows,int terminal_cols);

#endif
