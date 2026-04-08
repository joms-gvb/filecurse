/* TODO enhance how to navigate file system, now can only go up, opening a dir bin/ goes to /bin and not to ~/y/bin/ also cant follow dirs in a lot of cases 
 * perhabs add a functionto change down */

#include <ncursesw/ncurses.h>
#include <limits.h>
#include <wchar.h>
#include <locale.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <stdarg.h>

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

static WINDOW *win_list;
static WINDOW *win_preview;
static WINDOW *win_status;
static int list_top;

static void save_msg_str(char *buf, size_t bsize, const char *fmt, ...)
{
	va_list ap;
	if (bsize == 0)
		return;
	va_start(ap, fmt);
	vsnprintf(buf, bsize, fmt, ap);
	va_end(ap);

	buf[bsize - 1] = '\0';
}

static void ui_window_create(void)
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

static int path_join(const char *dir, const char *name, char *out, size_t outlen)
{
	int n;
	if (!dir || !name || !out)
		return -1;
	if (strcmp(dir, "/") == 0)
		n = snprintf(out, outlen, "/%s", name);
	else
		n = snprintf(out, outlen, "%s/%s", dir, name);

	return (n >= 0 && (size_t)n < outlen) ? 0 : -1;
}

static int change_into(const char **cur_dir_ptr, const char *child, char *buf, size_t buflen)
{
	if (!child || !cur_dir_ptr || !buf)
		return -1;

	if (path_join(*cur_dir_ptr, child, buf, buflen) != 0)
		return -1;
	
	if (chdir(buf) != 0)
		return -1;
	*cur_dir_ptr = buf;
	return 0;
}

static int change_up(const char **cur_dir_ptr, char *buf, size_t buflen)
{
	if (!cur_dir_ptr || !buf)
		return -1;
	
	if (!getcwd(buf, buflen))
		return -1;

	if (strcmp(buf, "/") == 0) {
		*cur_dir_ptr = buf;
		return 0;
	}

	char *p = strrchr(buf, '/');
	if (!p)
		return -1;
	
	if (p == buf)
		p[1]  = '\0';
	else
		*p = '\0';
	if (chdir(buf) != 0)
		return -1;

	*cur_dir_ptr = buf;
	return 0;
}


static void ui_init(void)
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

static void kill_ui(void)
{
	if (win_list)
		delwin(win_list);
	if (win_preview)
		delwin(win_preview);
	if (win_status)
		delwin(win_status);
	endwin();
}

static void ui_render_list(struct FileExplorer *exp, int top, int sel)
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

static void ui_render_preview(const char *path)
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

static wchar_t *utf8_to_wcs_conv(const char *s, size_t *out_len)
{
	wchar_t *buf = NULL;
	size_t cap = 0, len = 0;
	mbstate_t st;
	const unsigned char *p;
	size_t ret;
	wchar_t wc;

	if (!s)
		return NULL;

	memset(&st, 0, sizeof(st));
	p = (const unsigned char *)s;

	while (*p) {
		ret = mbrtowc(&wc, (const char *)p, MB_CUR_MAX, &st);
		if (ret == (size_t) -1 || ret == (size_t) -2) {
			errno = EILSEQ;
			free(buf);
			return NULL;
		}

		if (len + 1 >= cap) {
			size_t ncap = cap ? cap * 2 : 64;
			wchar_t *nb = realloc(buf, ncap * sizeof(*nb));
			if (!nb) {
				free(buf);
				return NULL;
			}
			buf = nb;
			cap = ncap;
		}
		buf[len++] = wc;
		if (ret == 0)
			break;
		p += ret;
	}
	if (!buf)  {
		buf = malloc(sizeof(*buf));
		if (!buf)
			return NULL;
		cap = 1;
	}
	buf[len] = L'\0';
	if (out_len)
		*out_len = len;
	return buf;
}


static void ui_status(const char *msg)
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

// TODO Work on utf8 suport

static void print_utf8_line(const char *utf8)
{
	wchar_t *wbuf = NULL;
	size_t needed;
	const char *src = utf8;
	mbstate_t st;

	if (!utf8)
		return;

	memset(&st, 0, sizeof(st));
	needed = mbsrtowcs(NULL, &src, 0, &st);

	if (needed == (size_t) - 1)
		return;

	wbuf = realloc(NULL, (needed + 1) * sizeof(*wbuf));
	if (!wbuf)
		return;
	memset(&st, 0, sizeof(st));
	src = utf8;
	if (mbsrtowcs(wbuf, &src, needed + 1, &st) ==  (size_t)-1) {
		free(wbuf);
		return;
	}

	addwstr(wbuf);
	free(wbuf);

}

void list_files(struct FileExplorer *exp, const char *dir_name)
{
	DIR *dir;
	struct dirent *entry;
	char fullpath[PATH_MAX];

	dir = opendir(dir_name);
	if (!dir) {
		perror("opendir");
		return;
	}

	exp->file_cnt = 0;

	while ((entry = readdir(dir)) != NULL) {
		if (exp->file_cnt >= MAX_FILES)
			break;
		/* skip . and .. */
		if (entry->d_name[0] == '.' && (entry->d_name[1] == '\0' ||
					(entry->d_name[1] == '.' && entry->d_name[2] == '\0')))
			continue;

		strncpy(exp->files[exp->file_cnt].name, entry->d_name, sizeof(exp->files[exp->file_cnt].name) - 1);

		exp->files[exp->file_cnt].name[sizeof(exp->files[exp->file_cnt].name) - 1] = '\0';

		/* default vals */
		exp->files[exp->file_cnt].is_dir = 0;
		exp->files[exp->file_cnt].is_exe = 0;
		exp->files[exp->file_cnt].mode = 0;

		if (entry->d_type == DT_DIR)
			exp->files[exp->file_cnt].is_dir = 1;

		/* build full path */
		if (snprintf(fullpath, sizeof(fullpath), "%s/%s", dir_name, entry->d_name) < (int)sizeof(fullpath)) {
			struct stat st;
			if (stat(fullpath, &st) == 0) {
				exp->files[exp->file_cnt].mode = st.st_mode;
				if (S_ISDIR(st.st_mode))
					exp->files[exp->file_cnt].is_dir = 1;
				/* exec bit ? */
				if (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH))
					exp->files[exp->file_cnt].is_exe = 1;
			}
		}

		exp->file_cnt++;
	}

	closedir(dir);
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

void pfile(const char *fname)
{
	FILE *file;
	char *line = NULL;
	size_t len = 0;
	ssize_t read;
	off_t *offsets = NULL;
	size_t cap = 0;
	size_t lcnt = 0;
	size_t top = 0;

	file = fopen(fname, "r");

	if (!file) {
		char ebuf[128];
		snprintf(ebuf, sizeof(ebuf), "ERROR: could not open %s", fname);

		ui_status(ebuf);
		return;
	}

	offsets = NULL;
	cap = 0;
	lcnt = 0;
	rewind(file);

	while ((read = getline(&line, &len, file)) != EOF) {
		if (lcnt + 1 > cap) {
			size_t ncap =  cap ? cap * 2 : 1024;
			off_t *tmp = realloc(offsets, ncap * sizeof(*offsets));
			if (!tmp)
				goto out_free;
			offsets = tmp;
			cap = ncap;
		}

		offsets[lcnt++] = ftell(file) - read;
	}

	for ( ;; ) {
		size_t displayed = 0;
		clear();

		if (top >= lcnt)
			top = lcnt ? lcnt - 1 : 0;
		if (lcnt) {
			if (fseeko(file, offsets[top], SEEK_SET) != 0)
				goto out_free;
			while (displayed < (size_t)(LINES - 1) && getline(&line, &len, file) != EOF) {
				/*printw("%s", line); */
				print_utf8_line(line);
				displayed++;
			}
		}

		printw("\nPress 'q' to quit, 'j' for down, 'k' for up.");

		int ch = getch();

		switch (ch) {
		case 'q':
		case 'Q':
			goto out_free;
			break;
		case 'j':
		case 'J':
		case KEY_DOWN:
			if (top + 1 < lcnt)
				top++;
			break;
		case 'k':
		case 'K':
		case KEY_UP:
			if (top > 0)
				top--;
			break;
		default:
			break;
		}

	}

out_free:
	free(line);
	free(offsets);
	fclose(file);
					
}



int editor_open(const char *fname)
{
	const char *editor_env;
	char *editor_dup = NULL;
	char **argv = NULL;
	size_t argc = 0;
	pid_t pid;
	int status = -1;
	struct sigaction sa_ignore, sa_oldint, sa_oldquit;
	int saved_errno = 0;
	if (!fname)
		return -1;

	editor_env = getenv("EDITOR");
	if (!editor_env || !*editor_env)
		editor_env = "vi";

	editor_dup = strdup(editor_env);
	if (!editor_dup)
	       return -1;

	{
		char *p = editor_dup;
		char *tok;
		
		while ((tok = strtok(p, " \t"))) {
			(void)tok;
			argc++;
			p = NULL;
		}
	}

	argv = calloc(argc + 2, sizeof(*argv));
	if (!argv) {
		free(editor_dup);
		return -1;
	}

	{
		size_t i = 0;
		char *p = editor_dup;
		char *tok;

		while ((tok = strtok(p, " \t"))) {
			argv[i++] = strdup(tok);
			p = NULL;
		}

		argv[argc] = (char *)fname;
		argv[argc + 1] = NULL;
	}

	sa_ignore.sa_handler = SIG_IGN;
	sigemptyset(&sa_ignore.sa_mask);
	sa_ignore.sa_flags = 0;
	sigaction(SIGINT, &sa_ignore, &sa_oldint);
	sigaction(SIGQUIT, &sa_ignore, &sa_oldquit);

	endwin();

	pid = fork();
	if (pid < 0) {
		saved_errno = errno;
		initscr();
		cbreak();
		noecho();
		keypad(stdscr, TRUE);
		sigaction(SIGINT, &sa_oldint, NULL);
		sigaction(SIGQUIT, &sa_oldquit, NULL);
		errno = saved_errno;
		status = -1;
		goto cleanup;
	}

	if (pid == 0) {
		struct sigaction sa_default;
	/*	size_t i; not in use*/	

		sa_default.sa_handler = SIG_DFL;
		sigemptyset(&sa_default.sa_mask);
		sa_default.sa_flags = 0;
		sigaction(SIGINT, &sa_default, NULL);
		sigaction(SIGQUIT, &sa_default, NULL);

		execvp(argv[0], argv);

		{
			char errbuf[256];
			int err = errno;
			snprintf(errbuf, sizeof(errbuf), "exec(%s) failed: %s\n", argv[0], strerror(err));
			write(STDERR_FILENO, errbuf, strlen(errbuf));
		}
		_exit(127);
	}

	while (waitpid(pid, &status, 0) < 0) {
		if (errno == EINTR)
			continue;
		status = -1;
		break;
	}

	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	sigaction(SIGINT, &sa_oldint, NULL);
	sigaction(SIGQUIT, &sa_oldquit, NULL);
cleanup:
	if (argv) {
		for (size_t i = 0; i < argc; i++)
			free(argv[i]);
		free(argv);
	}
	free(editor_dup);

	return status;
}


void status_print(const char *msg)
{
	move(LINES - 1, 0);
	clrtoeol();
	print_utf8_line(msg);
}


int main()
{
	struct FileExplorer exp = {.file_cnt = 0, .sel_file = 0 };
	char msg[256];
	int ch;

	setlocale(LC_ALL, "");

	char cur_dir_buf[PATH_MAX];
	const char *cur_dir = cur_dir_buf;

	if (!getcwd(cur_dir_buf, sizeof(cur_dir_buf)))
		strncpy(cur_dir_buf, ".", sizeof(cur_dir_buf));



	ui_init();

	for ( ;; ) {
		list_files(&exp, cur_dir);

		if (exp.file_cnt == 0)
			exp.sel_file = 0;
		else if (exp.sel_file >= exp.file_cnt)
			exp.sel_file = exp.file_cnt - 1;

		{
			int maxy = getmaxy(win_list) - 2;
			if (maxy < 1 )
				maxy = 1;
			if (exp.sel_file < list_top)
				list_top = exp.sel_file;
			else if ( exp.sel_file >= list_top + maxy)
				list_top = exp.sel_file - maxy + 1;
		}

		ui_render_list(&exp, list_top, exp.sel_file);

		if (exp.file_cnt && !exp.files[exp.sel_file].is_dir)
			ui_render_preview(exp.files[exp.sel_file].name);
		else
			ui_render_preview(NULL);

		snprintf(msg, sizeof(msg), "Use ↑/↓ or k/j to navigate, Enter to open, q to quit");
		ui_status(msg);

		ch = getch();

		switch (ch) {
		case KEY_UP:
		case 'k':
		case 'K':
			exp.sel_file = (exp.sel_file > 0 ) ? exp.sel_file - 1 : 0;
			break;
		case KEY_DOWN:
		case 'j':
		case 'J':
			exp.sel_file = (exp.sel_file < exp.file_cnt -1) ? exp.sel_file + 1 : (exp.file_cnt ? exp.file_cnt - 1 : 0);
			break;
		case '\n':
			if (exp.file_cnt == 0)
				break;
			/*if (exp.files[exp.sel_file].is_dir)
				cur_dir = exp.files[exp.sel_file].name;
				*/
			if (exp.files[exp.sel_file].is_dir) {
				if (change_into(&cur_dir, exp.files[exp.sel_file].name, cur_dir_buf, sizeof(cur_dir_buf)) != 0)
					ui_status("Cannot enter directory");
				else {
					exp.sel_file = 0;
					list_top = 0;
				}

			} else {
				save_msg_str(msg, sizeof(msg), "View or edit %s? (v/e)", exp.files[exp.sel_file].name);
				ui_status(msg);
				int choice = getch();
				if (choice == 'v' || choice == 'V')
					pfile(exp.files[exp.sel_file].name);
				else if (choice == 'e' || choice == 'E') {
					int rc = editor_open(exp.files[exp.sel_file].name);

					if (rc == -1)
						save_msg_str(msg, sizeof(msg), "Failed to launch editor for %s", exp.files[exp.sel_file].name);
					else if (rc != 0)
						save_msg_str(msg, sizeof(msg), "Editor exited with status %d for %s", rc, exp.files[exp.sel_file].name);
					else
						save_msg_str(msg, sizeof(msg), "Edited %s", exp.files[exp.sel_file].name);

					ui_status(msg);
					timeout(1000);
					timeout(-1);
				}
			}
			break;
		case KEY_BACKSPACE:
		case 127:
		case 'h':
		case 'H':
			if (change_up(&cur_dir, cur_dir_buf, sizeof(cur_dir_buf)) != 0)
				ui_status("Cannot go up");
			else {
				exp.sel_file = 0;
				list_top = 0;
			}
			break;
		case 'q':
		case 'Q':
			goto out;
		default:
			break;
		}
	}

out:
	kill_ui();
	return 0;
}
