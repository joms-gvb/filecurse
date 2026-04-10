#include <limits.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "fs.h"
#include "filecurse.h"
#include "ui.h"
#include "util.h" /* status_print() and other helpers if used */

int path_join(const char *dir, const char *name, char *out, size_t outlen)
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

int change_into(const char **cur_dir_ptr, const char *child, char *buf, size_t buflen)
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

int change_up(const char **cur_dir_ptr, char *buf, size_t buflen)
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
