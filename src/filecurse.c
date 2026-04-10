#include <locale.h>
#include "filecurse.h"
#include "editor.h"
#include "fs.h"
#include "ui.h"
#include "util.h"

WINDOW *win_list;
WINDOW *win_preview;
WINDOW *win_status;

int list_top;


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
