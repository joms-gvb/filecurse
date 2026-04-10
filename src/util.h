#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>
#include <wchar.h>
#include <stdarg.h>

/* static [?] */
void save_msg_str(char *buf, size_t bsize, const char *fmt, ...);

/* static [?] */
void print_utf8_line(const char *utf8);

/* static [?] */
wchar_t *utf8_to_wcs_conv(const char *s, size_t *out_len);

void status_print(const char *msg);

#endif /* UTIL_H */
