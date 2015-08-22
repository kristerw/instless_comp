#ifndef SCREEN_H
#define SCREEN_H

#include <stdarg.h>

void *memset(void *s, int c, unsigned int n);
void printf(const char *fmt, ...);

void clear_screen(void);

#endif
