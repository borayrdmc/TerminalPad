#include "renderer.h"
#include "../text-engine/selection/selection.h"
#include "../text-engine/buffer/buffer.h"
#include "../search/search.h"
#include "../file-manager/file_manager.h"

#include <stdio.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static char render_buf[65536];
static int render_pos=0;

int save_active=0;
int input_prompt_active=0;
char save_filename_buf[256]={0};

static void renderWrite(const char *text){
    int len=(int)strlen(text);
    if(render_pos + len<(int)sizeof(render_buf)){
        memcpy(render_buf + render_pos,text,len);
        render_pos+=len;
    }
}

static void renderChar(char ch){
    if(render_pos<(int)sizeof(render_buf)){
        render_buf[render_pos++]=ch;
    }
}

static void renderFlush(){
    if(render_pos<=0) return;
#ifdef _WIN32
    fwrite(render_buf,1,render_pos,stdout);
    fflush(stdout);
#else
    ssize_t written=write(STDOUT_FILENO,render_buf,render_pos);
    (void)written;
#endif
    render_pos=0;
}

static int utf8VisualLen(const char *text){
    int count=0;
    int i=0;
    while(text[i]!='\0'){
        i+=utf8_char_len((unsigned char)text[i]);
        count++;
    }
    return count;
}

//Normalize selection endpoints once per frame so per-char highlighting stays O(1)
static int resolveSelectionBounds(line_x **out_start_line,node_x **out_start_node,line_x **out_end_line,node_x **out_end_node){
    if(!selection_active || selection_start.line==NULL || selection_end.line==NULL){
        return 0;
    }

    line_x *a_line=selection_start.line;
    node_x *a_node=selection_start.node;
    line_x *b_line=selection_end.line;
    node_x *b_node=selection_end.node;

    if(a_line==b_line && a_node==b_node) return 0;

    int a_first=0;
    if(a_line==b_line){
        node_x *temp_node=a_node;
        while(temp_node!=NULL){
            if(temp_node==b_node){
                a_first=1;
                break;
            }
            temp_node=temp_node->next;
        }
    }else{
        line_x *temp_line=a_line;
        while(temp_line!=NULL){
            if(temp_line==b_line){
                a_first=1;
                break;
            }
            temp_line=temp_line->nextline;
        }
    }

    if(!a_first){
        line_x *swap_line=a_line;
        a_line=b_line;
        b_line=swap_line;

        node_x *swap_node=a_node;
        a_node=b_node;
        b_node=swap_node;
    }

    *out_start_line=a_line;
    *out_start_node=a_node;
    *out_end_line=b_line;
    *out_end_node=b_node;
    return 1;
}

void renderer_draw(int terminal_rows,int terminal_cols){
    if(terminal_rows<1) terminal_rows=24;
    if(terminal_cols<1) terminal_cols=80;

    render_pos=0;
    renderWrite("\033[?25l\033[H");

    int header_rows=2;
    int prompt_active=(save_active || exit_prompt_active || replace_active || search_active);
    int max_lines=terminal_rows - header_rows - (prompt_active ? 2 : 0);
    if(max_lines<1) max_lines=1;

    int current_row_idx=0;
    int cur_pos=0;
    getNodePosition(currentline,cursor,&current_row_idx,&cur_pos);
    int current_line_num=current_row_idx + 1;

    static int scroll_offset=0;
    static int h_scroll_offset=0;

    if(current_line_num>scroll_offset + max_lines){
        scroll_offset=current_line_num - max_lines;
    }
    if(current_line_num<=scroll_offset){
        scroll_offset=current_line_num - 1;
    }
    if(scroll_offset<0) scroll_offset=0;

    int text_cols=terminal_cols;
    if(cur_pos>=h_scroll_offset + text_cols){
        h_scroll_offset=cur_pos - text_cols + 1;
    }else if(cur_pos<h_scroll_offset){
        h_scroll_offset=cur_pos;
    }
    if(h_scroll_offset<0) h_scroll_offset=0;

    //1. Header
    renderWrite("\033[2K TerminalPad - V.1.0\r\n\033[2K");
    for(int i=0;i<terminal_cols;i++) renderChar('-');
    renderWrite("\r\n");

    //Precompute selection bounds and search cursor for O(1) per-node highlight checks
    line_x *sel_a_line=NULL;
    node_x *sel_a_node=NULL;
    line_x *sel_b_line=NULL;
    node_x *sel_b_node=NULL;
    int has_sel=resolveSelectionBounds(&sel_a_line,&sel_a_node,&sel_b_line,&sel_b_node);
    int sel_line_state=0; //0: before selection, 1: inside selection, 2: after selection

    SearchMatch *match_ptr=search_active ? search_matches : NULL;

    //2. Text area
    line_x *temp_line=headline;
    int line_num=1;
    while(temp_line!=NULL && line_num<=scroll_offset){
        if(has_sel){
            if(temp_line==sel_a_line) sel_line_state=1;
            if(temp_line==sel_b_line) sel_line_state=2;
        }
        temp_line=temp_line->nextline;
        line_num++;
    }

    int display_line=1;
    int cursor_row=header_rows + 1;
    int cursor_col=1;
    int line_idx=scroll_offset;

    while(temp_line!=NULL && display_line<=max_lines){
        if(has_sel && temp_line==sel_a_line){
            sel_line_state=1;
        }

        renderWrite("\033[2K");

        int skip=(temp_line==currentline) ? h_scroll_offset : 0;
        int is_start_line=(has_sel && temp_line==sel_a_line);
        int is_end_line=(has_sel && temp_line==sel_b_line);

        int node_in_sel=0;
        if(has_sel && sel_line_state==1){
            if(is_start_line){
                node_in_sel=(sel_a_node==temp_line->dummynode) ? 1 : 0;
            }else if(is_end_line){
                node_in_sel=(sel_b_node==temp_line->dummynode) ? 0 : 1;
            }else{
                node_in_sel=1;
            }
        }

        node_x *temp_node=temp_line->dummynode->next;
        int skipped=0;
        int col_idx=0;
        while(temp_node!=NULL && skipped<skip){
            if(has_sel && sel_line_state==1){
                if(is_start_line && temp_node==sel_a_node) node_in_sel=1;
                if(is_end_line && temp_node==sel_b_node) node_in_sel=0;
            }
            skipped++;
            temp_node=temp_node->next;
            col_idx++;
        }

        int shown=0;
        int current_bg=0;
        while(temp_node!=NULL && shown<text_cols){
            int needed_bg=0;
            if(node_in_sel){
                needed_bg=1;
            }else if(match_ptr!=NULL){
                while(match_ptr!=NULL &&
                      (match_ptr->line_index<line_idx ||
                      (match_ptr->line_index==line_idx && match_ptr->col + match_ptr->len<=col_idx))){
                    match_ptr=match_ptr->next;
                }
                if(match_ptr!=NULL && match_ptr->line_index==line_idx &&
                   col_idx>=match_ptr->col && col_idx<match_ptr->col + match_ptr->len){
                    needed_bg=2;
                }
            }

            if(needed_bg!=current_bg){
                if(needed_bg==1) renderWrite("\033[43m");
                else if(needed_bg==2) renderWrite("\033[44m");
                else renderWrite("\033[0m");
                current_bg=needed_bg;
            }

            renderWrite(temp_node->letter);

            if(has_sel && sel_line_state==1){
                if(is_start_line && temp_node==sel_a_node) node_in_sel=1;
                if(is_end_line && temp_node==sel_b_node) node_in_sel=0;
            }

            shown++;
            temp_node=temp_node->next;
            col_idx++;
        }

        if(current_bg!=0){
            renderWrite("\033[0m");
        }

        if(temp_line==currentline){
            cursor_row=header_rows + display_line;
            cursor_col=1 + (cur_pos - h_scroll_offset);
        }

        if(has_sel && temp_line==sel_b_line){
            sel_line_state=2;
        }

        if(prompt_active || display_line<max_lines){
            renderWrite("\r\n");
        }
        temp_line=temp_line->nextline;
        line_num++;
        display_line++;
        line_idx++;
    }

    for(int i=display_line;i<=max_lines;i++){
        renderWrite("\033[2K");
        if(prompt_active || i<max_lines){
            renderWrite("\r\n");
        }
    }

    //3. Interactive prompt bar (only rendered when active)
    if(prompt_active){
        renderWrite("\033[2K");
        for(int i=0;i<terminal_cols;i++) renderChar('-');
        renderWrite("\r\n\033[2K");

        if(save_active){
            char status[320];
            snprintf(status,sizeof(status)," Dosya adi: %s",save_filename_buf);
            renderWrite(status);
        }else if(exit_prompt_active){
            renderWrite(" Metni kaydetmek ister misiniz? (Y/N)");
        }else if(replace_active){
            char status[320];
            snprintf(status,sizeof(status)," Yeni kelime: %s",replace_query);
            renderWrite(status);
        }else if(search_active){
            char status[320];
            snprintf(status,sizeof(status)," Ara: %s",search_query);
            renderWrite(status);
        }
    }

    //4. Cursor positioning + atomic flush
    char pos_buf[64];
    if(save_active){
        snprintf(pos_buf,sizeof(pos_buf),"\033[%d;%dH",terminal_rows,13 + utf8VisualLen(save_filename_buf));
    }else if(input_prompt_active && replace_active){
        snprintf(pos_buf,sizeof(pos_buf),"\033[%d;%dH",terminal_rows,15 + utf8VisualLen(replace_query));
    }else if(input_prompt_active && search_active){
        snprintf(pos_buf,sizeof(pos_buf),"\033[%d;%dH",terminal_rows,7 + utf8VisualLen(search_query));
    }else if(exit_prompt_active){
        snprintf(pos_buf,sizeof(pos_buf),"\033[%d;%dH",terminal_rows,39);
    }else{
        snprintf(pos_buf,sizeof(pos_buf),"\033[%d;%dH",cursor_row,cursor_col);
    }
    renderWrite(pos_buf);
    renderWrite("\033[?25h");
    renderFlush();
}

void renderer_draw_filebrowser(int terminal_rows,int terminal_cols){
    if(terminal_rows<1) terminal_rows=24;
    if(terminal_cols<1) terminal_cols=80;

    render_pos=0;
    renderWrite("\033[?25l\033[H");

    //Header
    renderWrite("\033[2K");
    for(int i=0;i<terminal_cols;i++) renderChar('-');
    renderWrite("\r\n\033[2K");

    char header[256];
    snprintf(header,sizeof(header),
        " [DOSYA GEZGINI - %s]  |  YUK/AS: Gezin  |  ENTER: Sec  |  CTRL+S: Kaydet  |  ESC: Iptal\r\n",
        (browser_mode==1) ? "AC" : "KAYDET"
    );
    renderWrite(header);

    renderWrite("\033[2K");
    for(int i=0;i<terminal_cols;i++) renderChar('-');
    renderWrite("\r\n");

    //Reserve 3 header rows + 1 bottom border + 1 info row (+1 prompt row in Save mode)
    int reserved=(browser_mode==2) ? 6 : 5;
    int display_limit=terminal_rows - reserved;
    if(display_limit<1) display_limit=1;
    fm_set_display_limit(display_limit);

    int max_name_cols=terminal_cols - 7;
    if(max_name_cols<1) max_name_cols=1;

    int shown=0;
    for(int i=file_scroll;i<file_count && shown<display_limit;i++){
        char line[512];
        const char *prefix=(i==file_cursor) ? " -> " : "    ";
        if(file_list[i].is_folder){
            snprintf(line,sizeof(line),"\033[2K%s[%.*s]/\r\n",prefix,max_name_cols,file_list[i].name);
        }else{
            snprintf(line,sizeof(line),"\033[2K%s%.*s\r\n",prefix,max_name_cols,file_list[i].name);
        }
        renderWrite(line);
        shown++;
    }

    for(int i=shown;i<display_limit;i++){
        renderWrite("\033[2K\r\n");
    }

    //Footer
    renderWrite("\033[2K");
    for(int i=0;i<terminal_cols;i++) renderChar('-');
    renderWrite("\r\n\033[2K");

    char scroll_info[128];
    int total_visible_end=file_scroll + display_limit;
    if(total_visible_end>file_count) total_visible_end=file_count;

    snprintf(scroll_info,sizeof(scroll_info),
        " Dosya: %d/%d  |  Gorunen: %d-%d",
        file_count>0 ? file_cursor + 1 : 0,
        file_count,
        file_count>0 ? file_scroll + 1 : 0,
        total_visible_end
    );
    renderWrite(scroll_info);

    if(browser_mode==2){
        char prompt[512];
        snprintf(prompt,sizeof(prompt),"\r\n\033[2K Gezin ve CTRL+S ile kaydet | Dosya adi: %s",save_filename_buf);
        renderWrite(prompt);
    }

    renderWrite("\033[?25h");
    renderFlush();
}
