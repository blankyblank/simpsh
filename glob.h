#ifndef GLOB_H
#define GLOB_H

#include <stddef.h>

int ismetachar(const char *, size_t);
int globmatch(const char *restrict, const char *restrict);
int globexpand(const char *restrict, char ***);


#endif /* GLOB_H */
