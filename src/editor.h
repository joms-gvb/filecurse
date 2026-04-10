#ifndef EDITOR_H
#define EDITOR_H

/* Open fname in the user's editor (from $EDITOR, defaults to "vi").
 * Returns the chiled's exit status un success, -1 on error.
 */

int editor_open(const char *fname);

#endif /* EDITOR_H */

