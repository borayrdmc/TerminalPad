#include <stdlib.h>
#include <string.h>
#include "undo_stack.h"

UndoStack *undo=NULL;

static char *duplicateString(const char *src){
    if(src==NULL) return NULL;
    size_t len=strlen(src) + 1;
    char *copy=malloc(len);
    if(copy!=NULL){
        memcpy(copy,src,len);
    }
    return copy;
}

UndoStack *undo_stack_create(){
    UndoStack *stack=malloc(sizeof(UndoStack));
    if(stack==NULL) return NULL;

    stack->top=NULL;
    stack->size=0;
    return stack;
}

void undo_push(UndoStack *stack,OpType type,int line,int col,const char *text){
    if(stack==NULL) return;

    UndoRecord *rec=malloc(sizeof(UndoRecord));
    if(rec==NULL) return;

    rec->type=type;
    rec->line_index=line;
    rec->col=col;
    rec->text=duplicateString(text);
    rec->next=stack->top;
    stack->top=rec;
    stack->size++;
}

UndoRecord *undo_pop(UndoStack *stack){
    if(stack==NULL || stack->top==NULL) return NULL;

    UndoRecord *rec=stack->top;
    stack->top=rec->next;
    stack->size--;
    return rec;
}

void undo_record_destroy(UndoRecord *rec){
    if(rec==NULL) return;
    free(rec->text);
    free(rec);
}

void undo_stack_destroy(UndoStack *stack){
    if(stack==NULL) return;

    UndoRecord *cur=stack->top;
    while(cur!=NULL){
        UndoRecord *next=cur->next;
        undo_record_destroy(cur);
        cur=next;
    }
    free(stack);
}
