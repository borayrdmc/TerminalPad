#ifndef CURSOR_H
#define CURSOR_H

#include "../buffer/buffer.h"

int calculateCursorPosition();
void getNodePosition(line_x *target_line,node_x *target_node,int *out_row,int *out_col);
void moveCursorToLeft();
void moveCursorToRight();
void moveCursorToUp();
void moveCursorToDown();
void skipBlockRight();
void skipBlockLeft();

#endif