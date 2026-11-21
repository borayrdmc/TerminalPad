#ifndef BUFFER_H
#define BUFFER_H

#include "../../undo/undo_stack.h"

typedef struct node{
    char letter[5];
    struct node *prev;
    struct node *next;
} node_x;

typedef struct line{
    node_x *dummynode;
    struct line *prevline;
    struct line *nextline;
} line_x;

extern line_x *headline;
extern line_x *currentline;
extern node_x *cursor;
extern int undo_enabled;

//Core Functions
node_x *createnewnode(const char *letter);
line_x *createnewline(void);
void letterEntry(const char *entry);
void addNewLine(void);
void deleteLetter(void);
void deleteBlock();
void mergeLines(void);

//Util functions
int utf8_char_len(unsigned char first_byte);
void buffer_free(void);
void recordUndo(OpType type,const char *text);

#endif