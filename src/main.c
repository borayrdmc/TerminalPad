#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif

#include "terminal/terminal.h"
#include "text-engine/buffer/buffer.h"
#include "ui/renderer.h"
#include "undo/undo_stack.h"
#include "text-engine/cursor/cursor.h"
#include "text-engine/selection/selection.h"
#include "text-engine/clipboard/clipboard.h"
#include "file-manager/file_manager.h"
#include "search/search.h"

int browser_mode=0;
int exit_prompt_active=0;
int exit_after_save=0;

static void rawWrite(const char *text,int len){
#ifdef _WIN32
    fwrite(text,1,len,stdout);
    fflush(stdout);
#else
    ssize_t written=write(STDOUT_FILENO,text,len);
    (void)written;
#endif
}

static void readInput(char *buf,int max_len){
    int pos=(int)strlen(buf);
    input_prompt_active=1;

    while(1){
        int rows,cols;
        term_get_size(&rows,&cols);
        renderer_draw(rows,cols);

        char utf8_char[5];
        int key=term_read_key(utf8_char);

        if(key==KEY_ESC){
            buf[0]='\0';
            break;
        }
        if(key==KEY_ENTER) break;

        if(key==KEY_BACKSPACE){
            if(pos>0){
                pos--;
                while(pos>0 && (((unsigned char)buf[pos] & 0xC0)==0x80)) pos--;
                buf[pos]='\0';
            }
        }else if(key==KEY_UTF8 || (key>=32 && key<127)){
            int char_len=(int)strlen(utf8_char);
            if(pos + char_len<max_len){
                memcpy(buf + pos,utf8_char,char_len);
                pos+=char_len;
                buf[pos]='\0';
            }
        }
    }

    input_prompt_active=0;
}

static void clearSearchOnEdit(int *navigate_mode){
    if(search_active || replace_active){
        search_clear();
        *navigate_mode=0;
    }
}

int main(void){
    term_enable_raw_mode();
    rawWrite("\033[?1049h",8); //Switch to alternate screen buffer
    term_cursor_blink_on();

    headline=createnewline();
    currentline=headline;
    cursor=currentline->dummynode;
    undo=undo_stack_create();

    int rows=24;
    int cols=80;
    int running=1;
    int paste_in_progress=0;
    int paste_start_line=0;
    int paste_start_col=0;
    int navigate_mode=0;

    while(running){
        term_get_size(&rows,&cols);

        //File browser mode (1: Open, 2: Save As)
        if(browser_mode==1 || browser_mode==2){
            renderer_draw_filebrowser(rows,cols);
            int key=term_read_key(NULL);

            if(key==KEY_ESC){
                browser_mode=0;
                save_filename_buf[0]='\0';
                exit_after_save=0;
            }else if(key==KEY_UP){
                fm_cursor_up();
            }else if(key==KEY_DOWN){
                fm_cursor_down();
            }else if(key==KEY_ENTER && file_count>0){
                if(file_list[file_cursor].is_folder){
                    if(chdir(file_list[file_cursor].name)==0){
                        file_cursor=0;
                        file_scroll=0;
                        fm_read_dir();
                    }
                }else if(browser_mode==1){
                    fm_load_file(file_list[file_cursor].name);
                    browser_mode=0;
                }
            }else if(key==KEY_CTRL_S && browser_mode==2){
                if(save_filename_buf[0]!='\0'){
                    fm_save_file(save_filename_buf);
                    save_filename_buf[0]='\0';
                    browser_mode=0;
                    if(exit_after_save) running=0;
                }
            }
            continue;
        }

        renderer_draw(rows,cols);
        char utf8_char[5];
        int key=term_read_key(utf8_char);

        if(key==KEY_ESC){
            navigate_mode=0;
            if(search_active || replace_active){
                search_clear();
            }else if(exit_prompt_active){
                exit_prompt_active=0;
            }else{
                exit_prompt_active=1;
            }
            continue;
        }

        if(exit_prompt_active){
            if(key=='y' || key=='Y'){
                exit_prompt_active=0;
                key=KEY_CTRL_S;
                exit_after_save=1;
            }else if(key=='n' || key=='N'){
                running=0;
                continue;
            }else{
                continue;
            }
        }

        switch(key){
            case KEY_ENTER:
                clearSearchOnEdit(&navigate_mode);
                if(selection_active) deleteSelection();
                addNewLine();
                break;

            case KEY_TAB: {
                clearSearchOnEdit(&navigate_mode);
                if(selection_active) deleteSelection();

                int start_row=0;
                int start_col=0;
                getNodePosition(currentline,cursor,&start_row,&start_col);

                undo_enabled=0;
                for(int i=0;i<4;i++) letterEntry(" ");
                undo_enabled=1;

                int end_row=0;
                int end_col=0;
                getNodePosition(currentline,cursor,&end_row,&end_col);

                char undo_payload[128];
                snprintf(undo_payload,sizeof(undo_payload),"PASTE:%d:%d:%d:%d",start_row,start_col,end_row,end_col);
                undo_push(undo,OP_REPLACE,0,0,undo_payload);
                break;
            }

            case KEY_BACKSPACE:
                clearSearchOnEdit(&navigate_mode);
                if(selection_active) deleteSelection();
                else deleteLetter();
                break;

            case KEY_DELETE:
                clearSearchOnEdit(&navigate_mode);
                if(selection_active){
                    deleteSelection();
                }else if(cursor->next!=NULL){
                    moveCursorToRight();
                    deleteLetter();
                }else if(currentline->nextline!=NULL){
                    currentline=currentline->nextline;
                    cursor=currentline->dummynode;
                    deleteLetter();
                }
                break;

            case KEY_ALT_BACKSPACE:
            case KEY_CTRL_BACKSPACE:
                clearSearchOnEdit(&navigate_mode);
                if(selection_active) deleteSelection();
                else deleteBlock();
                break;

            case KEY_UP:
                if(navigate_mode && search_matches!=NULL){
                    search_prev();
                }else{
                    selection_clear();
                    moveCursorToUp();
                }
                break;

            case KEY_DOWN:
                if(navigate_mode && search_matches!=NULL){
                    search_next();
                }else{
                    selection_clear();
                    moveCursorToDown();
                }
                break;

            case KEY_LEFT:
                if(navigate_mode && search_matches!=NULL){
                    search_prev();
                }else{
                    selection_clear();
                    moveCursorToLeft();
                }
                break;

            case KEY_RIGHT:
                if(navigate_mode && search_matches!=NULL){
                    search_next();
                }else{
                    selection_clear();
                    moveCursorToRight();
                }
                break;

            case KEY_CTRL_LEFT:
                selection_clear();
                skipBlockLeft();
                break;

            case KEY_CTRL_RIGHT:
                selection_clear();
                skipBlockRight();
                break;

            case KEY_SHIFT_LEFT:
                selection_shift_left();
                break;

            case KEY_SHIFT_RIGHT:
                selection_shift_right();
                break;

            case KEY_CTRL_SHIFT_LEFT:
                selection_word_left();
                break;

            case KEY_CTRL_SHIFT_RIGHT:
                selection_word_right();
                break;

            case KEY_SHIFT_UP:
                selection_shift_up();
                break;

            case KEY_SHIFT_DOWN:
                selection_shift_down();
                break;

            case KEY_CTRL_C:
                copySelection();
                break;

            case KEY_CTRL_X:
                cutSelection();
                break;

            case KEY_CTRL_V:
                pasteClipboard();
                break;

            case KEY_CTRL_Z:
                performUndo();
                break;

            case KEY_CTRL_F: {
                search_clear();
                navigate_mode=0;
                search_active=1;
                readInput(search_query,sizeof(search_query));
                if(search_query[0]!='\0'){
                    search_find(search_query);
                    if(search_matches!=NULL) search_next();
                }else{
                    search_clear();
                }
                break;
            }

            case KEY_CTRL_G: {
                search_clear();
                navigate_mode=0;
                search_active=1;
                readInput(search_query,sizeof(search_query));
                if(search_query[0]!='\0'){
                    search_find(search_query);
                    if(search_matches!=NULL){
                        search_next();
                        navigate_mode=1;
                    }
                }else{
                    search_clear();
                }
                break;
            }

            case KEY_CTRL_R: {
                search_clear();
                navigate_mode=0;
                search_active=1;
                readInput(search_query,sizeof(search_query));
                if(search_query[0]=='\0'){
                    search_clear();
                    break;
                }
                search_active=0;
                replace_active=1;
                readInput(replace_query,sizeof(replace_query));
                if(replace_query[0]!='\0'){
                    search_replace_all(search_query,replace_query);
                }
                search_clear();
                break;
            }

            case KEY_CTRL_O: {
                file_cursor=0;
                file_scroll=0;
                fm_read_dir();
                browser_mode=1;
                break;
            }

            case KEY_CTRL_S: {
                if(current_filename[0]!='\0'){
                    fm_save_file(current_filename);
                    if(exit_after_save) running=0;
                }else{
                    save_active=1;
                    save_filename_buf[0]='\0';
                    readInput(save_filename_buf,sizeof(save_filename_buf));
                    save_active=0;
                    if(save_filename_buf[0]!='\0'){
                        file_cursor=0;
                        file_scroll=0;
                        fm_read_dir();
                        browser_mode=2;
                    }else{
                        exit_after_save=0;
                    }
                }
                break;
            }

            case KEY_CTRL_W: {
                save_active=1;
                save_filename_buf[0]='\0';
                readInput(save_filename_buf,sizeof(save_filename_buf));
                save_active=0;
                if(save_filename_buf[0]!='\0'){
                    file_cursor=0;
                    file_scroll=0;
                    fm_read_dir();
                    browser_mode=2;
                }
                break;
            }

            case KEY_PASTE_START: {
                paste_in_progress=1;
                undo_enabled=0;
                getNodePosition(currentline,cursor,&paste_start_line,&paste_start_col);
                break;
            }

            case KEY_PASTE_END: {
                undo_enabled=1;
                if(paste_in_progress){
                    paste_in_progress=0;
                    int end_line=0;
                    int end_col=0;
                    getNodePosition(currentline,cursor,&end_line,&end_col);

                    char undo_payload[128];
                    snprintf(undo_payload,sizeof(undo_payload),"PASTE:%d:%d:%d:%d",
                             paste_start_line,paste_start_col,end_line,end_col);
                    undo_push(undo,OP_REPLACE,0,0,undo_payload);
                }
                break;
            }

            case KEY_UTF8:
                clearSearchOnEdit(&navigate_mode);
                if(selection_active) deleteSelection();
                letterEntry(utf8_char);
                break;

            default:
                if(key>=32 && key<127){
                    clearSearchOnEdit(&navigate_mode);
                    if(selection_active) deleteSelection();
                    letterEntry(utf8_char);
                }
                break;
        }
    }

    buffer_free();
    undo_stack_destroy(undo);
    term_cursor_blink_off();
    term_reset_color();
    term_show_cursor();
    term_disable_raw_mode();
    rawWrite("\033[?1049l",8); //Restore primary screen buffer
    return 0;
}
