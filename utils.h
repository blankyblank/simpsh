/* util.h - misc helper functions */
#ifndef UTILS_H
#define UTILS_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "malloc.h"

/** null terminated memcpy */
#define nmemcpy(d,s,l) do { \
  memcpy(d, s, l); \
  d[l] = '\0'; \
} while (0)

/**  check if char is whitespace  */
/* is_ws  WARN: for future me. use the posix isspace function instead */

/**  check if char is operator  */
#define is_operator(c) (c == '&' || c == '|' || c == ';' || c == '(' || c == ')')

/**  check if char is line end  */
#define is_cmd_end(c) ((c == ' ') | (c == '\t') | (c == '\n'))

/**  get length of char* array  */
static inline size_t
array_len(char **arr)
{
  size_t n = 0;
  while (arr[n])
    n++;
  return n;
}

/**  skip whitespace  */
static inline const char *
skip_ws(const char *s)
{
  while (isspace(*s))
    s++;
  return s;
}

/**  replicate strchrnul  */
static inline char *
s_strchrnul(const char *s, int c)
{
  while (*s && *s != c)
    s++;
  return (char *)s;
}

/**  strndup using memcpy  */
static inline char *
s_strndup(const char *s, size_t n)
{
  char *dup;
  
  if ((dup = malloc(n + 1))) {
    memcpy(dup, s, n);
    dup[n] = '\0';
  }
  return dup;
}

/** strcat using memcpy */
static inline char *
s_strcat(char *dest, const char *src)
{
  size_t destlen = strlen(dest), srclen = strlen(src) + 1;
  memcpy(dest + destlen, src, srclen);
  return dest;
}

/** mempcpy implementation */
__attribute__((always_inline)) static inline char *
s_mempcpy(char *dest, const char *src, size_t n)
{
  memcpy(dest, src, n);
  return dest + n;
}

/**  strdup using memcpy  */
static inline char *
s_strdup(const char *s)
{
  size_t len;
  char *dup;
  len = strlen(s);
  if ((dup = malloc(len + 1))) {
    memcpy(dup, s, len);
    dup[len] = '\0';
  }
  return dup;
}

/**  free char* array  */
static inline void
freeptr(char **args)
{
  if (args != NULL) {
    size_t i = 0;
    while (args[i] != NULL) {
      free(args[i]);
      i++;
    }
    free(args);
  }
}

/**  hash string to put in hash table  */
static inline unsigned int
hash(const char *s, unsigned int buckets)
{
  unsigned long int h = 525201411107845655ull;
  while (*s) {
    h ^= (unsigned char)*s;
    h *= 0x5bd1e9955bd1e995;
    h ^= h >> 47;
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
  eq = s_strchrnul(assn, '=');
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
st_read_assn(const char *assn, char **name, char **value)
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

#endif /* !UTILS_H */
