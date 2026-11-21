#ifndef SELECTION_H
#define SELECTION_H

#include "../buffer/buffer.h"

typedef struct {
    line_x *line;
    node_x *node;
} SelectionPoint;

extern int selection_active;

extern SelectionPoint selection_start;
extern SelectionPoint selection_end;

void initiate_selection();
void selection_clear();
int selection_contains(line_x *line,node_x *node);

void selection_shift_left();
void selection_shift_right();
void selection_shift_up();
void selection_shift_down();
void selection_word_right();
void selection_word_left();

#endif