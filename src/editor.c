#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdio.h>
#include <ncursesw/ncurses.h>

#include "editor.h"

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
