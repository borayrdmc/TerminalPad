#include "clipboard.h"
#include "../buffer/buffer.h"
#include "../cursor/cursor.h"
#include "../selection/selection.h"
#include "../../undo/undo_stack.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
#endif

#define CLIPBOARD_MAX 65536

static char clipboard_buf[CLIPBOARD_MAX]={0};
static int clipboard_len=0;

//OS clipboard bridge (Windows API / no-op on Linux)
static void systemClipboardSet(const char *text,int text_len){
#ifdef _WIN32
    if(text==NULL || text_len<=0) return;

    int wide_len=MultiByteToWideChar(CP_UTF8,0,text,text_len,NULL,0);
    if(wide_len<=0) return;

    HGLOBAL global_mem=GlobalAlloc(GMEM_MOVEABLE,(wide_len + 1) * sizeof(WCHAR));
    if(global_mem==NULL) return;

    WCHAR *mem_ptr=(WCHAR *)GlobalLock(global_mem);
    if(mem_ptr==NULL){
        GlobalFree(global_mem);
        return;
    }

    MultiByteToWideChar(CP_UTF8,0,text,text_len,mem_ptr,wide_len);
    mem_ptr[wide_len]=L'\0';
    GlobalUnlock(global_mem);

    if(OpenClipboard(NULL)){
        EmptyClipboard();
        SetClipboardData(CF_UNICODETEXT,global_mem);
        CloseClipboard();
    }else{
        GlobalFree(global_mem);
    }
#else
    (void)text;
    (void)text_len;
#endif
}

static int systemClipboardGet(char *out_buf,int max_len){
#ifdef _WIN32
    if(out_buf==NULL || max_len<=1) return 0;
    if(!OpenClipboard(NULL)) return 0;

    HANDLE clip_data=GetClipboardData(CF_UNICODETEXT);
    if(clip_data==NULL){
        CloseClipboard();
        return 0;
    }

    WCHAR *wide_text=(WCHAR *)GlobalLock(clip_data);
    if(wide_text==NULL){
        CloseClipboard();
        return 0;
    }

    char temp_buf[CLIPBOARD_MAX];
    int byte_count=WideCharToMultiByte(CP_UTF8,0,wide_text,-1,temp_buf,sizeof(temp_buf) - 1,NULL,NULL);
    GlobalUnlock(clip_data);
    CloseClipboard();

    if(byte_count<=0) return 0;
    byte_count--; //Exclude null terminator

    //Normalize CRLF to LF
    int out_pos=0;
    for(int i=0;i<byte_count && out_pos<max_len - 1;i++){
        if(temp_buf[i]=='\r') continue;
        out_buf[out_pos++]=temp_buf[i];
    }
    out_buf[out_pos]='\0';
    return out_pos;
#else
    (void)out_buf;
    (void)max_len;
    return 0;
#endif
}

//Helper functions
static int nodeComesBefore(node_x *first_node,node_x *second_node){
    node_x *temp_node=first_node;
    while(temp_node!=NULL){
        if(temp_node==second_node) return 1;
        temp_node=temp_node->next;
    }
    return 0;
}

static int lineComesBefore(line_x *first_line,line_x *second_line){
    line_x *temp_line=first_line;
    while(temp_line!=NULL){
        if(temp_line==second_line) return 1;
        temp_line=temp_line->nextline;
    }
    return 0;
}

static void normalizeSelection(line_x **start_line,node_x **start_node,line_x **end_line,node_x **end_node){
    int start_comes_first=(*start_line==*end_line)
        ? nodeComesBefore(*start_node,*end_node)
        : lineComesBefore(*start_line,*end_line);

    if(!start_comes_first){
        line_x *swap_line=*start_line;
        *start_line=*end_line;
        *end_line=swap_line;

        node_x *swap_node=*start_node;
        *start_node=*end_node;
        *end_node=swap_node;
    }
}

static line_x *getLineAt(int row_index){
    line_x *temp_line=headline;
    for(int i=0;i<row_index && temp_line!=NULL;i++){
        temp_line=temp_line->nextline;
    }
    return temp_line;
}

static node_x *getNodeAt(line_x *target_line,int col_index){
    node_x *temp_node=target_line->dummynode;
    for(int i=0;i<col_index && temp_node->next!=NULL;i++){
        temp_node=temp_node->next;
    }
    return temp_node;
}

static void insertUtf8Text(const char *text,int text_len){
    int i=0;
    while(i<text_len && text[i]!='\0'){
        if(text[i]=='\n'){
            addNewLine();
            i++;
        }else{
            int char_len=utf8_char_len((unsigned char)text[i]);
            if(i + char_len>text_len) char_len=text_len - i;

            char utf8_char[5]={0};
            memcpy(utf8_char,text + i,char_len);
            letterEntry(utf8_char);
            i+=char_len;
        }
    }
}

static int extractSelectionText(line_x *start_line,node_x *start_node,line_x *end_line,node_x *end_node,char *out_buf,int max_len){
    int out_len=0;
    line_x *current_line=start_line;

    while(current_line!=NULL && out_len<max_len - 1){
        node_x *first_node=(current_line==start_line) ? start_node->next : current_line->dummynode->next;
        node_x *stop_node=(current_line==end_line) ? end_node->next : NULL;

        for(node_x *temp_node=first_node;temp_node!=stop_node;temp_node=temp_node->next){
            int char_len=(int)strlen(temp_node->letter);
            if(out_len + char_len>=max_len - 1) break;

            memcpy(out_buf + out_len,temp_node->letter,char_len);
            out_len+=char_len;
        }

        if(current_line!=end_line && out_len<max_len - 1){
            out_buf[out_len++]='\n';
        }
        if(current_line==end_line) break;

        current_line=current_line->nextline;
    }

    out_buf[out_len]='\0';
    return out_len;
}

//Selection & clipboard operations
void deleteSelection(){
    if(!selection_active) return;

    line_x *start_line=selection_start.line;
    node_x *start_node=selection_start.node;
    line_x *end_line=selection_end.line;
    node_x *end_node=selection_end.node;
    normalizeSelection(&start_line,&start_node,&end_line,&end_node);

    if(start_line==end_line && start_node==end_node){
        selection_clear();
        return;
    }

    //Record deleted selection as a single OP_REPLACE undo entry ("row:col:text")
    if(undo && undo_enabled){
        static char deleted_buf[CLIPBOARD_MAX];
        int deleted_len=extractSelectionText(start_line,start_node,end_line,end_node,deleted_buf,CLIPBOARD_MAX);
        int start_row=0;
        int start_col=0;
        getNodePosition(start_line,start_node,&start_row,&start_col);

        char *undo_payload=malloc(32 + deleted_len + 1);
        if(undo_payload!=NULL){
            int prefix_len=snprintf(undo_payload,32,"%d:%d:",start_row,start_col);
            memcpy(undo_payload + prefix_len,deleted_buf,deleted_len + 1);
            undo_push(undo,OP_REPLACE,0,0,undo_payload);
            free(undo_payload);
        }
    }

    currentline=start_line;
    cursor=start_node;

    if(start_line==end_line){
        node_x *stop_node=end_node->next;
        node_x *current_node=start_node->next;

        while(current_node!=NULL && current_node!=stop_node){
            node_x *next_node=current_node->next;
            start_node->next=next_node;
            if(next_node!=NULL) next_node->prev=start_node;
            free(current_node);
            current_node=next_node;
        }
    }else{
        //Detach remaining nodes on end_line before freeing intermediate lines
        node_x *tail_nodes=end_node->next;
        end_node->next=NULL;
        line_x *after_end_line=end_line->nextline;

        node_x *current_node=start_node->next;
        while(current_node!=NULL){
            node_x *next_node=current_node->next;
            free(current_node);
            current_node=next_node;
        }

        start_node->next=tail_nodes;
        if(tail_nodes!=NULL) tail_nodes->prev=start_node;

        line_x *deleted_line=start_line->nextline;
        while(deleted_line!=NULL && deleted_line!=after_end_line){
            line_x *next_line=deleted_line->nextline;
            node_x *temp_node=deleted_line->dummynode;
            while(temp_node!=NULL){
                node_x *next_node=temp_node->next;
                free(temp_node);
                temp_node=next_node;
            }
            free(deleted_line);
            deleted_line=next_line;
        }

        start_line->nextline=after_end_line;
        if(after_end_line!=NULL) after_end_line->prevline=start_line;
    }

    selection_clear();
}

void copySelection(){
    if(!selection_active) return;

    line_x *start_line=selection_start.line;
    node_x *start_node=selection_start.node;
    line_x *end_line=selection_end.line;
    node_x *end_node=selection_end.node;
    normalizeSelection(&start_line,&start_node,&end_line,&end_node);

    clipboard_len=extractSelectionText(start_line,start_node,end_line,end_node,clipboard_buf,CLIPBOARD_MAX);
    systemClipboardSet(clipboard_buf,clipboard_len);
}

void cutSelection(){
    if(!selection_active) return;
    copySelection();
    deleteSelection();
}

void pasteClipboard(){
    //Sync from OS clipboard if available
    static char sys_buf[CLIPBOARD_MAX];
    int sys_len=systemClipboardGet(sys_buf,CLIPBOARD_MAX);
    if(sys_len>0){
        memcpy(clipboard_buf,sys_buf,sys_len);
        clipboard_buf[sys_len]='\0';
        clipboard_len=sys_len;
    }

    if(clipboard_len==0) return;

    int start_row,start_col;
    getNodePosition(currentline,cursor,&start_row,&start_col);

    undo_enabled=0;
    insertUtf8Text(clipboard_buf,clipboard_len);
    undo_enabled=1;

    int end_row,end_col;
    getNodePosition(currentline,cursor,&end_row,&end_col);

    char undo_payload[128];
    snprintf(undo_payload,sizeof(undo_payload),"PASTE:%d:%d:%d:%d",start_row,start_col,end_row,end_col);
    undo_push(undo,OP_REPLACE,0,0,undo_payload);
}

//Undo execution
void performUndo(){
    if(!undo) return;
    UndoRecord *record=undo_pop(undo);
    if(!record) return;

    undo_enabled=0;

    switch(record->type){
        case OP_INSERT_CHAR: {
            line_x *target_line=getLineAt(record->line_index);
            if(target_line==NULL) break;

            currentline=target_line;
            cursor=getNodeAt(currentline,record->col);

            if(cursor->next!=NULL){
                node_x *deleted_node=cursor->next;
                cursor->next=deleted_node->next;
                if(deleted_node->next!=NULL) deleted_node->next->prev=cursor;
                free(deleted_node);
            }
            break;
        }
        case OP_DELETE_CHAR: {
            line_x *target_line=getLineAt(record->line_index);
            if(target_line==NULL) break;

            currentline=target_line;
            cursor=getNodeAt(currentline,record->col - 1);

            if(record->text!=NULL) letterEntry(record->text);
            break;
        }
        case OP_INSERT_LINE: {
            line_x *target_line=getLineAt(record->line_index);
            if(target_line==NULL) break;

            currentline=target_line->nextline;
            if(currentline!=NULL){
                cursor=currentline->dummynode;
                mergeLines();
            }
            break;
        }
        case OP_DELETE_LINE: {
            line_x *target_line=getLineAt(record->line_index);
            if(target_line==NULL) break;

            currentline=target_line;
            cursor=currentline->dummynode;
            while(cursor->next!=NULL) cursor=cursor->next;
            addNewLine();
            break;
        }
        case OP_REPLACE: {
            if(record->text==NULL) break;

            if(strncmp(record->text,"PASTE:",6)==0){
                int start_row,start_col,end_row,end_col;
                sscanf(record->text + 6,"%d:%d:%d:%d",&start_row,&start_col,&end_row,&end_col);

                line_x *start_line=getLineAt(start_row);
                line_x *end_line=getLineAt(end_row);
                if(start_line==NULL || end_line==NULL) break;

                selection_start.line=start_line;
                selection_start.node=getNodeAt(start_line,start_col);
                selection_end.line=end_line;
                selection_end.node=getNodeAt(end_line,end_col);
                selection_active=1;
                deleteSelection();
            }else{
                //Restore cut text encoded as "row:col:text"
                const char *text_ptr=record->text;
                int start_row=atoi(text_ptr);
                text_ptr=strchr(text_ptr,':');
                if(text_ptr==NULL) break;
                text_ptr++;

                int start_col=atoi(text_ptr);
                text_ptr=strchr(text_ptr,':');
                if(text_ptr==NULL) break;
                text_ptr++;

                line_x *target_line=getLineAt(start_row);
                if(target_line==NULL) break;

                currentline=target_line;
                cursor=getNodeAt(currentline,start_col);
                insertUtf8Text(text_ptr,(int)strlen(text_ptr));
            }
            break;
        }
        default: break;
    }

    undo_enabled=1;
    undo_record_destroy(record);
}
