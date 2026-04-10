#ifndef FILECURSES_H
#define FILECURSES_H

#include <sys/types.h>
#include <ncursesw/ncurses.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#define MAX_FILES 1024

struct FileInfo {
	char name[256];
	int is_dir;
	mode_t mode;
	int is_exe;
};

struct FileExplorer {
	struct FileInfo files[MAX_FILES];
	int file_cnt;
	int sel_file;
};

/* ncurses windows and UI states shared across modules */

extern WINDOW *win_list;
extern WINDOW *win_preview;
extern WINDOW *win_status;
extern int list_top;

#endif /* FILECURSES_H */

