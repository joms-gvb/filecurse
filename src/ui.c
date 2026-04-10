#include <locale.h>
#include <ncursesw/ncurses.h>
#include <string.h>
#include <stdlib.h>

#include "ui.h"
#include "filecurse.h"
#include "util.h"

void ui_init(void)
{
	initscr();
	start_color();
	use_default_colors();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(2, COLOR_CYAN, -1);
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	mousemask(ALL_MOUSE_EVENTS | REPORT_MOUSE_POSITION, NULL);
	list_top = 0;
	ui_window_create();
}

void ui_window_create(void)
{
	int h, w;

	getmaxyx(stdscr, h, w);

	if (win_list) {
		delwin(win_list);
		delwin(win_preview);
		delwin(win_status);
	}

	win_list    = newwin(h - 2, w / 2, 0, 0);
	win_preview = newwin(h - 2, w - w / 2, 0, w / 2);
	win_status  = newwin(2, w, h - 2, 0);

	box(win_list, 0, 0);
	box(win_preview, 0, 0);
	wrefresh(win_list);
	wrefresh(win_preview);
	wrefresh(win_status);
}

void ui_status(const char *msg)
{
	wchar_t *wmsg;
	int maxw;
	if (!msg)
		msg = "";

	wbkgdset(win_status, COLOR_PAIR(1));
	werase(win_status);
	wbkgd(win_status, ' ' | COLOR_PAIR(1));

	wmsg = utf8_to_wcs_conv(msg, NULL);
	maxw = getmaxx(win_status) - 2;

	if (maxw < 0)
		maxw = 0;
	if  (wmsg) {
		mvwaddnwstr(win_status, 0, 1, wmsg, maxw);
		free(wmsg);
	} else
		mvwprintw(win_status, 0, 1, "%s", msg);

	wrefresh(win_status);

}

void ui_render_list(struct FileExplorer *exp, int top, int sel)
{
	int maxy, maxx;
	int y = 1; /* leave 0 for box */

	getmaxyx(win_list, maxy, maxx);
	werase(win_list);
	box(win_list, 0, 0);

	for (int i = top; i < exp->file_cnt && y < maxy - 1; i++, y++) {
		const char *name = exp->files[i].name;
		int avail = maxx - 4;

		int namelen = (avail > 0) ? avail : 0;

		if (i == sel)
			wattron(win_list, A_REVERSE);

		if (exp->files[i].is_dir) {
			wattron(win_list, COLOR_PAIR(2));
			if (namelen > 0)
				mvwprintw(win_list, y, 2, "%.*s/", namelen - 1, name);
			else
				mvwprintw(win_list, y, 2, "/");
			wattroff(win_list, COLOR_PAIR(2));
		} else { 
			if (exp->files[i].is_exe) {
				wattron(win_list, A_BOLD);
				if (namelen > 0)
					mvwprintw(win_list, y, 2, "%.*s*", namelen - 1, name);
				else
					mvwprintw(win_list, y, 2, "*");

				wattroff(win_list, A_BOLD);
			} else {
				if (namelen > 0)
					mvwprintw(win_list, y, 2, "%.*s", namelen, name);
				else
					mvwprintw(win_list, y, 2, "%s", "");
			}
		}

		if (i == sel)
			wattroff(win_list, A_REVERSE);
	}

	wrefresh(win_list);
}

void ui_render_preview(const char *path)
{
	FILE *f;

	char buf[512];
	int y = 1, maxy, maxx;
	getmaxyx(win_preview, maxy, maxx);
	werase(win_preview);
	box(win_preview, 0, 0);

	if (!path) {
		mvwprintw(win_preview, 1, 2, "No file selected");
		wrefresh(win_preview);
		return;
	}
	f = fopen(path, "r");
	if (!f) {
		mvwprintw(win_preview, 1, 2, "Cannot open: %s", path);
		wrefresh(win_preview);
		return;
	}

	while (y < maxy - 1 && fgets(buf, sizeof(buf), f)) {
		buf[maxx - 4] = '\0';
		mvwprintw(win_preview, y++, 2, "%s", buf);
	}

	fclose(f);
	wrefresh(win_preview);
}

void display_files(struct FileExplorer *exp)
{
	for (int i = 0; i < exp->file_cnt; i++) {
		if (i == exp->sel_file) {
			attron(COLOR_PAIR(1));
			attron(A_REVERSE);
		}
		printw("%s%s\n", exp->files[i].name, (exp->files[i].is_dir) ? "/" : "");

		if (i == exp->sel_file) {
			attroff(A_REVERSE);
			attroff(COLOR_PAIR(1));
		}


	}

}

void kill_ui(void)
{
	if (win_list)
		delwin(win_list);
	if (win_preview)
		delwin(win_preview);
	if (win_status)
		delwin(win_status);
	endwin();
}
