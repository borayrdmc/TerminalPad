#include "selection.h"
#include "stddef.h"
#include "../cursor/cursor.h"

int selection_active=0;
SelectionPoint selection_start={NULL,NULL};
SelectionPoint selection_end={NULL,NULL};

void initiate_selection(){

    if(!selection_active){
        selection_start.line=currentline;
        selection_start.node=cursor;
        selection_end.line=currentline;
        selection_end.node=cursor;
        selection_active=1;
    }
}

void selection_clear(){
    selection_active=0;
    selection_start.line=NULL;
    selection_start.node=NULL;
    selection_end.line=NULL;
    selection_end.node=NULL;
}

int selection_contains(line_x *searched_line,node_x *searched_node){ //If node is in selection check
    
    if(!selection_active) return 0;

    node_x *selection_start_node=selection_start.node;
    line_x *selection_start_line=selection_start.line;

    node_x *selection_end_node=selection_end.node;    
    line_x *selection_end_line=selection_end.line;

    int start_comes_before=0;

    //SWAP BLOCK
    if(selection_start_line==selection_end_line){ //Same line selection - moved left or right?
        
        node_x *temp_node=selection_start_node;

        while(temp_node!=NULL){ //move to right, if came across end it means we moved right, else it means we moved left start_comes_before stays 0 

            if(temp_node==selection_end_node){ 
                start_comes_before=1;
                break; 
            }
            temp_node=temp_node->next;
        }
    }

    else{ //Multiline selection

        line_x *temp_line=selection_start_line;

        while(temp_line!=NULL){ //Same logic with nodes but up and down

            if(temp_line==selection_end_line){ 
                start_comes_before=1; 
                break; 
            }
            temp_line=temp_line->nextline;
        }
    }

    if(!start_comes_before){ //Swap start and end

        line_x *swap_line=selection_start_line; 
        selection_start_line=selection_end_line; 
        selection_end_line=swap_line;

        node_x *swap_node=selection_start_node; 
        selection_start_node=selection_end_node; 
        selection_end_node=swap_node;
    }

    //SEARCH BLOCK
    line_x *temp_line=selection_start_line;

    //Line based search (if this line selected)
    while(temp_line!=NULL){

        if(temp_line==searched_line){ //We found our line in between start line and end line (or its directly our line) so it means its selected
            break; 
        }
        if(temp_line==selection_end_line){
            return 0;
        }
        temp_line=temp_line->nextline;
    }

    //Node based seatch (if this line is selected, is this node selected too?)
    if(selection_start_line==selection_end_line && searched_line==selection_start_line){ //Same line selection

        node_x *temp_node=selection_start_node->next;

        while(temp_node!=NULL && temp_node!=selection_end_node->next){

            if(temp_node==searched_node){
                return 1;
            }
            temp_node=temp_node->next;
        }
        return 0;
    }

    if(searched_line==selection_start_line){ //Start line in multiline selection

        node_x *temp_node=selection_start_node->next;

        while(temp_node!=NULL){

            if(temp_node==searched_node){
                return 1;
            }
            temp_node=temp_node->next;
        }
        return 0;
    }

    if(searched_line==selection_end_line){ //End line in multiline selection

        node_x *temp_node=selection_end_line->dummynode->next;

        while(temp_node!=NULL && temp_node!=selection_end_node->next){

            if(temp_node==searched_node){
                return 1;
            }
            temp_node=temp_node->next;
        }
        return 0;
    }

    //Lines inbetween are definately selected
    return 1;
}

//Selection move functions
void selection_shift_left(){
    initiate_selection();
    moveCursorToLeft();
    selection_end.line=currentline;
    selection_end.node=cursor;
}

void selection_shift_right(){
    initiate_selection();
    moveCursorToRight();
    selection_end.line=currentline;
    selection_end.node=cursor;
}

void selection_shift_up(){
    initiate_selection();
    moveCursorToUp();
    selection_end.line=currentline;
    selection_end.node=cursor;
}

void selection_shift_down(){
    initiate_selection();
    moveCursorToDown();
    selection_end.line=currentline;
    selection_end.node=cursor;
}

void selection_word_right(){
    initiate_selection();
    skipBlockRight();
    selection_end.line=currentline;
    selection_end.node=cursor;
}

void selection_word_left(){
    initiate_selection();
    skipBlockLeft();
    selection_end.line=currentline;
    selection_end.node=cursor;
}