#ifndef FILE_MANAGER_H
#define FILE_MANAGER_H

#define MAX_FILE_COUNT     512
#define MAX_PATH_LEN       1024

typedef struct {
    char name[256];
    int is_folder;
} FileEntry;

extern FileEntry file_list[MAX_FILE_COUNT];
extern int file_count;
extern int file_cursor;
extern int file_scroll;
extern int fm_display_limit; /* renderer tarafindan set edilir */
extern char current_filename[MAX_PATH_LEN];

//File browser functions
void fm_read_dir();
void fm_cursor_up();
void fm_cursor_down();

//File operation functions
void fm_load_file(const char *filename);
void fm_save_file(const char *filename);

//Util functions
int is_absolute_path(const char *path);
void set_full_path(const char *filename);
int compareFileEntries(const void *a,const void *b);
void fm_set_display_limit(int limit);
void fm_clear_editor();

#endif