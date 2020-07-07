/* See LICENSE file for copyright and license details. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __OpenBSD__
#include <unistd.h>
#endif /* __OpenBSD__ */

#include "util.h"

static void
verr(const char *fmt, va_list ap)
{
	if (argv0 && strncmp(fmt, "usage", sizeof("usage") - 1)) {
		fprintf(stderr, "%s: ", argv0);
	}

	vfprintf(stderr, fmt, ap);

	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}
}

void
warn(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(fmt, ap);
	va_end(ap);
}

void
die(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	verr(fmt, ap);
	va_end(ap);

	exit(1);
}

void
epledge(const char *promises, const char *execpromises)
{
	(void)promises;
	(void)execpromises;

#ifdef __OpenBSD__
	if (pledge(promises, execpromises) == -1) {
		die("pledge:");
	}
#endif /* __OpenBSD__ */
}

void
eunveil(const char *path, const char *permissions)
{
	(void)path;
	(void)permissions;

#ifdef __OpenBSD__
	if (unveil(path, permissions) == -1) {
		die("unveil:");
	}
#endif /* __OpenBSD__ */
}
