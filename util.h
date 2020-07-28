/* See LICENSE file for copyright and license details. */
#ifndef UTIL_H
#define UTIL_H

#undef MIN
#define MIN(x,y)  ((x) < (y) ? (x) : (y))
#undef MAX
#define MAX(x,y)  ((x) > (y) ? (x) : (y))
#undef LEN
#define LEN(x) (sizeof (x) / sizeof *(x))

extern char *argv0;

void warn(const char *, ...);
void die(const char *, ...);
void tdie(struct tls *ctx, const char *fmt, ...);
void tcdie(struct tls_config *conf, const char *fmt, ...);

void epledge(const char *, const char *);
void eunveil(const char *, const char *);

#endif /* UTIL_H */
