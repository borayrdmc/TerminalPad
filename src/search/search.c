#include "search.h"
#include "../text-engine/buffer/buffer.h"
#include "../text-engine/selection/selection.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

int search_active=0;
int replace_active=0;
char search_query[256]={0};
char replace_query[256]={0};
SearchMatch *search_matches=NULL;
SearchMatch *current_match=NULL;

static void matchesClear(){
    SearchMatch *current=search_matches;
    while(current!=NULL){
        SearchMatch *next=current->next;
        free(current);
        current=next;
    }
    search_matches=NULL;
    current_match=NULL;
}

static int nodesMatchCaseInsensitive(node_x *start_node,const char *query,int *matched_nodes){
    node_x *current=start_node;
    const char *query_ptr=query;
    int count=0;

    while(*query_ptr!='\0'){
        if(current==NULL) return 0;
        int char_len=utf8_char_len((unsigned char)*query_ptr);
        if(char_len==1){
            if(current->letter[1]!='\0' || tolower((unsigned char)current->letter[0])!=tolower((unsigned char)*query_ptr)){
                return 0;
            }
        }else{
            if(strncmp(current->letter,query_ptr,char_len)!=0 || current->letter[char_len]!='\0'){
                return 0;
            }
        }
        query_ptr+=char_len;
        current=current->next;
        count++;
    }

    *matched_nodes=count;
    return 1;
}

static void moveCursorToMatch(SearchMatch *match){
    if(match==NULL) return;

    line_x *line=headline;
    for(int i=0;i<match->line_index && line!=NULL;i++){
        line=line->nextline;
    }
    if(line==NULL) return;

    currentline=line;
    cursor=currentline->dummynode;
    //Position cursor at the end of the matched word
    for(int i=0;i<match->col + match->len;i++){
        if(cursor->next!=NULL) cursor=cursor->next;
    }
}

void search_find(const char *query){
    matchesClear();
    if(query==NULL || query[0]=='\0') return;

    SearchMatch *tail=NULL;
    int line_idx=0;
    line_x *line=headline;

    while(line!=NULL){
        int col=0;
        node_x *current=line->dummynode->next;
        while(current!=NULL){
            int match_len=0;
            if(nodesMatchCaseInsensitive(current,query,&match_len)){
                SearchMatch *match=malloc(sizeof(SearchMatch));
                if(match==NULL) return;

                match->line_index=line_idx;
                match->col=col;
                match->len=match_len;
                match->next=NULL;

                if(search_matches==NULL){
                    search_matches=match;
                }else{
                    tail->next=match;
                }
                tail=match;
            }
            current=current->next;
            col++;
        }

        line=line->nextline;
        line_idx++;
    }
}

void search_clear(){
    matchesClear();
    search_active=0;
    replace_active=0;
    search_query[0]='\0';
    replace_query[0]='\0';
}

void search_next(){
    if(search_matches==NULL) return;

    if(current_match==NULL || current_match->next==NULL){
        current_match=search_matches;
    }else{
        current_match=current_match->next;
    }

    moveCursorToMatch(current_match);
}

void search_prev(){
    if(search_matches==NULL) return;

    if(current_match==NULL || current_match==search_matches){
        SearchMatch *last=search_matches;
        while(last->next!=NULL) last=last->next;
        current_match=last;
    }else{
        SearchMatch *prev=search_matches;
        while(prev!=NULL && prev->next!=current_match) prev=prev->next;
        if(prev==NULL) return;
        current_match=prev;
    }

    moveCursorToMatch(current_match);
}

int search_contains(int line_index,int col){
    if(!search_active) return 0;

    SearchMatch *match=search_matches;
    while(match!=NULL){
        if(match->line_index==line_index && col>=match->col && col<match->col + match->len){
            return 1;
        }
        match=match->next;
    }
    return 0;
}

void search_replace_all(const char *query,const char *replacement){
    if(query==NULL || query[0]=='\0' || replacement==NULL) return;

    search_find(query);
    if(search_matches==NULL) return;

    selection_clear();

    //Collect non-overlapping matches so patterns like "aa" in "aaaa" replace cleanly
    int count=0;
    SearchMatch *match=search_matches;
    while(match!=NULL){
        count++;
        match=match->next;
    }

    SearchMatch **matches_arr=malloc(count * sizeof(SearchMatch *));
    if(matches_arr==NULL){
        matchesClear();
        return;
    }

    int filtered_count=0;
    int last_line=-1;
    int last_end_col=-1;
    match=search_matches;
    while(match!=NULL){
        if(match->line_index!=last_line || match->col>=last_end_col){
            matches_arr[filtered_count++]=match;
            last_line=match->line_index;
            last_end_col=match->col + match->len;
        }
        match=match->next;
    }

    int rep_len=(int)strlen(replacement);

    //Replace from end to start so earlier column indices stay valid
    for(int i=filtered_count - 1;i>=0;i--){
        line_x *line=headline;
        for(int j=0;j<matches_arr[i]->line_index && line!=NULL;j++){
            line=line->nextline;
        }
        if(line==NULL) continue;

        node_x *cur=line->dummynode;
        for(int j=0;j<matches_arr[i]->col;j++){
            if(cur->next!=NULL) cur=cur->next;
        }

        for(int j=0;j<matches_arr[i]->len;j++){
            if(cur->next!=NULL){
                node_x *deleted_node=cur->next;
                cur->next=deleted_node->next;
                if(deleted_node->next!=NULL) deleted_node->next->prev=cur;
                free(deleted_node);
            }
        }

        currentline=line;
        cursor=cur;
        int j=0;
        while(j<rep_len){
            int char_len=utf8_char_len((unsigned char)replacement[j]);
            if(j + char_len>rep_len) char_len=rep_len - j;

            char utf8_char[5]={0};
            memcpy(utf8_char,replacement + j,char_len);

            node_x *newnode=createnewnode(utf8_char);
            newnode->next=cursor->next;
            newnode->prev=cursor;
            if(cursor->next!=NULL) cursor->next->prev=newnode;
            cursor->next=newnode;
            cursor=newnode;
            j+=char_len;
        }
    }

    free(matches_arr);
    matchesClear();
}
