#include "file_manager.h"
#include "../text-engine/buffer/buffer.h"
#include "../text-engine/selection/selection.h"
#include "../search/search.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#ifdef _WIN32
#  include <direct.h>
#  define GETCWD _getcwd
#  define PATH_SEP '\\'
#else
#  include <unistd.h>
#  define GETCWD getcwd
#  define PATH_SEP '/'
#endif

FileEntry file_list[MAX_FILE_COUNT];
int file_count=0;
int file_cursor=0;
int file_scroll=0;
int fm_display_limit=20;
char current_filename[MAX_PATH_LEN]="";

//File browser function
void fm_read_dir(){

    DIR *dir=opendir(".");
    struct dirent *entry;
    struct stat st;

    file_count=0;

    if(dir==NULL) return;

    while((entry=readdir(dir))!=NULL && file_count<MAX_FILE_COUNT){

        if(strcmp(entry->d_name,".")==0) continue;

        strncpy(file_list[file_count].name,entry->d_name,255);
        file_list[file_count].name[255]='\0';

        if(stat(entry->d_name,&st)==0){
            file_list[file_count].is_folder=S_ISDIR(st.st_mode);
        }
        else{
            file_list[file_count].is_folder=0;
        }
        file_count++;
    }

    closedir(dir);

    if(file_count>1){
        qsort(file_list,file_count,sizeof(FileEntry),compareFileEntries);
    }
}

void fm_cursor_up(){ //File browser cursor up
    if(file_count<=0) return;

    if(file_cursor>0){
        file_cursor--;
        if(file_cursor<file_scroll)
            file_scroll--;
    }else{
        file_cursor=file_count - 1;
        file_scroll=file_count - fm_display_limit;
        if(file_scroll<0) file_scroll=0;
    }
}

void fm_cursor_down(){ //File browser cursor down

    if(file_count<=0) return;

    if(file_cursor<file_count - 1){

        file_cursor++;

        if(file_cursor>=file_scroll + fm_display_limit){
            file_scroll++;
        }
    }
    else{
        file_cursor=0;
        file_scroll=0;
    }
}

//File operation functions
void fm_load_file(const char *filename){

    FILE *fp=fopen(filename,"rb");
    if(fp==NULL) return;

    set_full_path(filename);
    fm_clear_editor();

    if(undo){
        undo_stack_destroy(undo);
        undo=undo_stack_create();
    }

    undo_enabled=0; //Undo enabled temporarily for performance. Doesnt calculate in each letter

    int currentCharacter;
    while((currentCharacter=fgetc(fp))!=EOF){

        if(currentCharacter=='\r') continue;
        else if(currentCharacter=='\n') addNewLine();

        else{
            char ch[5]={0};
            int len=utf8_char_len((unsigned char)currentCharacter);
            ch[0]=(char)currentCharacter;

            for(int i=1;i<len;i++){
                int next_c=fgetc(fp);
                if(next_c==EOF) break;
                ch[i]=(char)next_c;
            }
            letterEntry(ch);
        }
    }
    undo_enabled=1;

    fclose(fp);

    cursor=headline->dummynode;
    currentline=headline;
}

void fm_save_file(const char *filename){

    if(filename==NULL || filename[0]=='\0') return;

    FILE *fp=fopen(filename,"w");
    if(fp==NULL) return;

    set_full_path(filename);

    line_x *currentLine=headline;

    while(currentLine!=NULL){

        node_x *currentNode=currentLine->dummynode->next;

        while(currentNode!=NULL){
            fputs(currentNode->letter,fp);
            currentNode=currentNode->next;
        }
        if(currentLine->nextline!=NULL)
            fputc('\n',fp);
        currentLine=currentLine->nextline;
    }
    fclose(fp);
}

//Util functions
int is_absolute_path(const char *path){

    #ifdef _WIN32
        if(path[0]!='\0' && path[1]==':') return 1; // C:\User\example.txt absolute path
        if(path[0]=='\\' || path[0]=='/') return 1; // /home/example.txt absolute path
        return 0;
    #else
        return path[0]=='/';
    #endif
}

void set_full_path(const char *filename){

    if(filename==NULL || filename[0]=='\0') return;

    char full[MAX_PATH_LEN];

    if(is_absolute_path(filename)){ //If absolute path copy directly
        strncpy(full,filename,MAX_PATH_LEN - 1);
        full[MAX_PATH_LEN - 1]='\0';
    }

    else{
        char cwd[MAX_PATH_LEN];

        if(GETCWD(cwd,sizeof(cwd))!=NULL){

            size_t cwd_len=strlen(cwd);
            size_t fname_len=strlen(filename);
            int has_sep=(cwd_len>0 && (cwd[cwd_len - 1]=='\\' || cwd[cwd_len - 1]=='/'));
            size_t sep_len=has_sep ? 0 : 1;

            if(cwd_len + sep_len + fname_len<sizeof(full)){
                memcpy(full,cwd,cwd_len);
                if(!has_sep) full[cwd_len]=PATH_SEP;
                memcpy(full + cwd_len + sep_len,filename,fname_len + 1);
            }else{
                strncpy(full,filename,MAX_PATH_LEN - 1);
                full[MAX_PATH_LEN - 1]='\0';
            }
        }
        else{
            strncpy(full,filename,MAX_PATH_LEN - 1);
            full[MAX_PATH_LEN - 1]='\0';
        }
    }

    strncpy(current_filename,full,MAX_PATH_LEN - 1);
    current_filename[MAX_PATH_LEN - 1]='\0';
}

int compareFileEntries(const void *a,const void *b){ //Sorting algoritm for file browser ->  .. > folders > files (alphabetically sorted) 
    
    const FileEntry *fa=(const FileEntry *)a;
    const FileEntry *fb=(const FileEntry *)b;

    int a_parent=(strcmp(fa->name,"..")==0);
    int b_parent=(strcmp(fb->name,"..")==0);
    if(a_parent && b_parent) return 0;
    if(a_parent) return -1;
    if(b_parent) return 1;

    if(fa->is_folder!=fb->is_folder) return fb->is_folder - fa->is_folder;

    return strcmp(fa->name,fb->name);
}

void fm_clear_editor(){
    selection_clear();
    search_clear();
    buffer_free();
    headline=createnewline();
    currentline=headline;
    cursor=currentline->dummynode;
}

void fm_set_display_limit(int limit){
    if(limit>0) fm_display_limit=limit;
}