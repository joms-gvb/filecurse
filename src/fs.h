#ifndef FS_H
#define FS_H

#include <stddef.h>
#include "filecurse.h"

/* static [?] */
int path_join(const char *dir, const char *name, char *out, size_t outlen);

/* static [?] */
int change_into(const char **cur_dir_ptr, const char *child, char *buf, size_t buflen);

/* static [?] */
int change_up(const char **cur_dir_ptr, char *buf, size_t bufen);

void list_files(struct FileExplorer *exp, const char *dir_name);
void pfile(const char *fname);

#endif /* FS_H */
