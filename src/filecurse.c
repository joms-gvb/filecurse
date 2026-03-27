#include <ncurses.h>
#include <wchar.h>
#include <locale.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#define MAX_FILES 1024

struct FileInfo {
	char name[256];
	int is_dir;
};

struct FileExplorer {
	struct FileInfo files[MAX_FILES];
	int file_cnt;
	int sel_file;
};

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

	dir = opendir(dir_name);
	if (!dir) {
		perror("opendir");
		return;
	}

	exp->file_cnt = 0;

	while ((entry = readdir(dir)) != NULL) {
		if (exp->file_cnt >= MAX_FILES)
			break;

		strncpy(exp->files[exp->file_cnt].name, entry->d_name, sizeof(exp->files[exp->file_cnt].name));
		exp->files[exp->file_cnt].is_dir = (entry->d_type == DT_DIR);
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
		printw("ERROR: Could not open file %s\n", fname);
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
		size_t i;

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
	setlocale(LC_ALL, "");

	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	cbreak();
	noecho();
	keypad(stdscr, TRUE);

	struct FileExplorer exp = {0};
	char *cur_dir = ".";

	char msg[256];
	for ( ;; ) {
		clear();

		list_files(&exp, cur_dir);
		display_files(&exp);

		snprintf(msg, sizeof(msg), "Use ↑/↓ or k/j to navigate, Enter to open, q to quit");
		status_print(msg);

		int ch = getch();

		switch (ch) {
		case KEY_UP:
		case 'k':
		case 'K':
			exp.sel_file = (exp.sel_file > 0) ? exp.sel_file - 1 : 0;
			break;
		case KEY_DOWN:
		case 'j':
		case 'J':
			exp.sel_file = (exp.sel_file < exp.file_cnt - 1) ? exp.sel_file + 1 : exp.file_cnt -1;
			break;
		case '\n':
			if (exp.files[exp.sel_file].is_dir)
				cur_dir = exp.files[exp.sel_file].name;
			else {
				snprintf(msg, sizeof(msg), "View or edit %s? (v/e)", exp.files[exp.sel_file].name);
				status_print(msg);
				int choice = getch();


				if (choice == 'v' || choice == 'V')
					pfile(exp.files[exp.sel_file].name);
				else if (choice == 'e' || choice == 'E') {
					int rc = editor_open(exp.files[exp.sel_file].name);

					if (rc == -1)
						snprintf(msg, sizeof(msg), "Failed to launch editor for %s", exp.files[exp.sel_file].name);
					else if (rc != 0)
						snprintf(msg, sizeof(msg), "Editor exited with status %d for %s", rc, exp.files[exp.sel_file].name);
					else
						snprintf(msg, sizeof(msg), "Edited %s", exp.files[exp.sel_file].name);
					status_print(msg);
					timeout(1000);
					getch();
					timeout(-1);
				}
			}


			break;
		case 'q':
			 goto out;
		
		}
	}

out:
	endwin();
	return 0;
}
