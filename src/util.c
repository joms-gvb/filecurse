#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <wchar.h>
#include <locale.h>
#include <errno.h>

#include "util.h"
#include "filecurse.h"

void save_msg_str(char *buf, size_t bsize, const char *fmt, ...)
{
	va_list ap;
	if (bsize == 0)
		return;
	va_start(ap, fmt);
	vsnprintf(buf, bsize, fmt, ap);
	va_end(ap);

	buf[bsize - 1] = '\0';
}

void print_utf8_line(const char *utf8)
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

wchar_t *utf8_to_wcs_conv(const char *s, size_t *out_len)
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

void status_print(const char *msg)
{
	move(LINES - 1, 0);
	clrtoeol();
	print_utf8_line(msg);
}

