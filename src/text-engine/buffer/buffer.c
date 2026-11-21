#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "buffer.h"
#include "../cursor/cursor.h"
#include "../../undo/undo_stack.h"

line_x *headline=NULL;
line_x *currentline=NULL;
node_x *cursor=NULL;

int undo_enabled=1;

node_x *createnewnode(const char *letter_input){

    node_x *newnode=malloc(sizeof(node_x));

    strncpy(newnode->letter,letter_input,4); //Copy each byte of input
    newnode->letter[4]='\0';

    newnode->prev=NULL;
    newnode->next=NULL;

    return newnode;
}

line_x *createnewline(){

    line_x *newline=malloc(sizeof(line_x));

    newline->dummynode=createnewnode("");
    newline->nextline=NULL;
    newline->prevline=NULL;

    return newline;
}

void letterEntry(const char *entry){

    recordUndo(OP_INSERT_CHAR,entry);

    node_x *newnode=createnewnode(entry);
    newnode->next=cursor->next;
    newnode->prev=cursor;

    if(cursor->next!=NULL){
        cursor->next->prev=newnode;
    }

    cursor->next=newnode;
    cursor=newnode;
}

void addNewLine(){

    recordUndo(OP_INSERT_LINE,NULL);

    line_x *newline=createnewline();

    newline->prevline=currentline;
    newline->nextline=currentline->nextline;

    if(currentline->nextline!=NULL){
        currentline->nextline->prevline=newline;
    }
    
    currentline->nextline=newline;

    if(cursor->next!=NULL){
        newline->dummynode->next=cursor->next;
        cursor->next->prev=newline->dummynode;
        cursor->next=NULL;
    }

    currentline=newline;
    cursor=currentline->dummynode;
}

void deleteLetter(){

    if(cursor==currentline->dummynode){

        if(currentline->prevline!=NULL){
            recordUndo(OP_DELETE_LINE,NULL);
            mergeLines();
        }
        return;
    }

    node_x *deleted_node=cursor;
    node_x *cursor_target_node=cursor->prev;

    if(cursor_target_node==NULL) return;

    recordUndo(OP_DELETE_CHAR,cursor->letter);

    if(cursor->next!=NULL){
        cursor->next->prev=cursor->prev;
    }
    cursor->prev->next=cursor->next;

    cursor=cursor_target_node;
    free(deleted_node);
}

void mergeLines(){

    if(currentline->prevline==NULL) return;

    line_x *deletedLine=currentline;
    line_x *upperLine=currentline->prevline;
    
    node_x *upperLastNode=upperLine->dummynode;

    while(upperLastNode->next!=NULL)
        upperLastNode=upperLastNode->next;

    if(deletedLine->dummynode->next!=NULL){
        upperLastNode->next=deletedLine->dummynode->next;
        deletedLine->dummynode->next->prev=upperLastNode;
    }

    upperLine->nextline=deletedLine->nextline;

    if(deletedLine->nextline!=NULL){
        deletedLine->nextline->prevline=upperLine;
    }

    cursor=upperLastNode;
    currentline=upperLine;

    free(deletedLine->dummynode);
    free(deletedLine);
}

void deleteBlock(){

    if(cursor==currentline->dummynode){
        if(currentline->prevline!=NULL){
            deleteLetter();
        }
        return;
    }

    //If space then delete spaces until a word comes up
    if(isspace((unsigned char)cursor->letter[0])){
        while(cursor!=currentline->dummynode && isspace((unsigned char)cursor->letter[0])){
            deleteLetter();
        }
    }
    //Delete letters until a space comes up
    else{
        while(cursor!=currentline->dummynode && !isspace((unsigned char)cursor->letter[0])){
            deleteLetter();
        }
    }
}

//Util functions
int utf8_char_len(unsigned char first_byte_of_input){

    //Bitwise comparison on first bytes to detect byte count of input
    if((first_byte_of_input & 0x80)==0x00) return 1; //ASCII
    if((first_byte_of_input & 0xE0)==0xC0) return 2; //Turkish Characters
    if((first_byte_of_input & 0xF0)==0xE0) return 3; //Symbols
    if((first_byte_of_input & 0xF8)==0xF0) return 4; //Emojis
    return 1;
}

void buffer_free(){ //Clear whole doubly linked list structure
    
    line_x *current_line=headline;

    while(current_line!=NULL){

        node_x *current_node=current_line->dummynode;

        while(current_node!=NULL){

            node_x *next_node=current_node->next;
            free(current_node);
            current_node=next_node;
        }

        line_x *next_line=current_line->nextline;
        free(current_line);
        current_line=next_line;
    }
    headline=NULL;
    currentline=NULL;
    cursor=NULL;
}

void recordUndo(OpType type,const char *text){

    if(!undo || !undo_enabled) return;

    int line=0;
    int col=0;
    if(type==OP_INSERT_CHAR || type==OP_DELETE_CHAR){
        getNodePosition(currentline,cursor,&line,&col);
    }else{
        getNodePosition(currentline,cursor,&line,NULL);
    }
    undo_push(undo,type,line,col,text);
}