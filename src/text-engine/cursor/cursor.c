#include <ctype.h>
#include <stddef.h>
#include "../buffer/buffer.h"
#include "cursor.h"

int calculateCursorPosition(){

    int cursor_position=0;
    node_x *temp=currentline->dummynode;

    while(temp!=cursor && temp->next!=NULL){
        temp=temp->next;
        cursor_position++;
    }

    return cursor_position;
}

void getNodePosition(line_x *target_line,node_x *target_node,int *out_row,int *out_col){
    if(out_row!=NULL){
        int row=0;
        line_x *temp_line=headline;
        while(temp_line!=NULL && temp_line!=target_line){
            row++;
            temp_line=temp_line->nextline;
        }
        *out_row=row;
    }

    if(out_col!=NULL){
        int col=0;
        node_x *temp_node=target_line->dummynode;
        while(temp_node!=NULL && temp_node!=target_node){
            col++;
            temp_node=temp_node->next;
        }
        *out_col=col;
    }
}

void moveCursorToLeft(){
    if(cursor==currentline->dummynode) return;
    cursor=cursor->prev;
}

void moveCursorToRight(){
    if(cursor->next==NULL) return;
    cursor=cursor->next;
}

void moveCursorToUp(){

    int target_position=calculateCursorPosition();

    if(currentline->prevline==NULL) return;

    cursor=currentline->prevline->dummynode;
    int current_position=0;

    while(cursor->next!=NULL && current_position<target_position){
        cursor=cursor->next;
        current_position++;
    }
    currentline=currentline->prevline;
}

void moveCursorToDown(){

    int target_position=calculateCursorPosition();

    if(currentline->nextline==NULL) return;

    cursor=currentline->nextline->dummynode;

    int current_position=0;

    while(cursor->next!=NULL && current_position<target_position){
        cursor=cursor->next;
        current_position++;
    }
    currentline=currentline->nextline;
}

void skipBlockRight(){
    if(cursor->next==NULL && currentline->nextline!=NULL){
        currentline=currentline->nextline;
        cursor=currentline->dummynode;
        return;
    }
    if(cursor->next==NULL) return;

    if(isspace((unsigned char)cursor->next->letter[0])){
        while(cursor->next!=NULL && isspace((unsigned char)cursor->next->letter[0])){
            moveCursorToRight();
        }
    }else{
        while(cursor->next!=NULL && !isspace((unsigned char)cursor->next->letter[0])){
            moveCursorToRight();
        }
    }
}

void skipBlockLeft(){
    if(cursor==currentline->dummynode && currentline->prevline!=NULL){
        currentline=currentline->prevline;
        cursor=currentline->dummynode;
        while(cursor->next!=NULL){
            cursor=cursor->next;
        }
        return;
    }
    if(cursor==currentline->dummynode) return;

    if(isspace((unsigned char)cursor->letter[0])){
        while(cursor!=currentline->dummynode && isspace((unsigned char)cursor->letter[0])){
            moveCursorToLeft();
        }
    }else{
        while(cursor!=currentline->dummynode && !isspace((unsigned char)cursor->letter[0])){
            moveCursorToLeft();
        }
    }
}