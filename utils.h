/* util.h - misc helper functions */
#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "malloc.h"
/* malloc and free or other small inlined functions */

/** get length of char* array */
static inline size_t
array_len(char **arr)
{
  size_t n = 0;
  while (arr[n])
    n++;
  return n;
}

/** check if char is whitespace */
static inline int
is_ws(char c)
{
  return c == ' ' || c == '\t' || c == '\n';
}

/** skip whitespace */
static inline const char *
skip_ws(const char *s)
{
  while (is_ws(*s))
    s++;
  return s;
}

/**  strndup using memcpy  */
static inline char *
s_strndup(const char *s, size_t n)
{
  char *dup = malloc(n + 1);
  if (dup) {
    memcpy(dup, s, n);
    dup[n] = '\0';
  }
  return dup;
}

/**  strdup using memcpy  */
static inline char *
s_strdup(const char *s)
{
  size_t len = strlen(s);
  char *dup = malloc(len + 1);
  if (dup) {
    memcpy(dup, s, len);
    dup[len] = '\0';
  }
  return dup;
}

/** reallocate buffer to bufsize */
static inline char *
s_realloc(char *buf, size_t *bufsize)
{
  char *t;

  *bufsize *= 2;
  t = realloc(buf, *bufsize);
  if (!t) {
    free(buf);
    return NULL;
  }
  return t;
}

/** free char* array */
static inline void
freeptr(char **args)
{
  if (args != NULL) {
    int i = 0;
    while (args[i] != NULL) {
      free(args[i]);
      i++;
    }
    free(args);
  }
}

/** hash string to put in hash table */
static inline unsigned int
hash(const char *s, unsigned int buckets)
{
  unsigned int h = 0;
  while (*s) {
    h = h * 31 + (unsigned char)*s;
    s++;
  }
  return h % buckets;
}

/**  brief find name and value from name=value pair  */
static inline void
read_assn(const char *assn, char **name, char **value)
{
  char *eq;
  int l;
  eq = (char *)strchr(assn, '=');
  if (!eq) {
    *name = s_strdup(assn);
    *value = NULL;
  } else {
    l = eq - assn;
    *name = s_strndup(assn, l);
    *value = s_strdup(eq + 1);
  }
}

/**  stack-based version - for temporary parsing  */
static inline void
read_assn_stack(const char *assn, char **name, char **value)
{
  char *eq;
  int l;
  eq = (char *)strchr(assn, '=');
  if (!eq) {
    *name = st_strdup(assn);
    *value = NULL;
  } else {
    l = eq - assn;
    *name = st_strndup(assn, l);
    *value = st_strdup(eq + 1);
  }
}

/* vim: set filetype=c: */
#endif /* !UTILS_H */
