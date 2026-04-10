#ifndef UI_H
#define UI_H

#include <stddef.h>
#include "filecurse.h"

/* static [?] */
void ui_init(void);

/* static [?] */
void ui_window_create(void);

/* static [?] */
void ui_status(const char *msg);

/* static [?] */
void ui_render_list(struct FileExplorer *exp, int top, int sel);

/* static [?] */
void ui_render_preview(const char *path);

void display_files(struct FileExplorer *exp);

/* static [?] */
void kill_ui(void);

#endif /* UI_H */
