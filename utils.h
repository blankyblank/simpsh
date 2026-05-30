/* util.h - misc helper functions */
#ifndef UTILS_H
#define UTILS_H

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "malloc.h"

/* error message macros/funcs */

/* err but it returns instead of exiting */
#define err(r,s) { warn(s); return r; }
/* errx but it returns instead of exiting */
#define errx(r,s) { fprintf(stderr, "%s\n", s); return r; }
/* error (returns doesn't exit) with simpsh: builtin: message: (errno message), format */
/* be careful in if statements */
#define sherr(r, b, m) warn("%s: %s", b, m); return r
/* same as sherr  but doesn't return */
#define shwarn(b, m) warn("%s: %s: %s", sh_argv0, b, m)
/* warning with simpsh: builtin: message, format */
#define shwarnx(b, m) fprintf(stderr, "%s: %s: %s\n", sh_argv0, b, m)
/* warning with simpsh: builtin: arg: message, format */
#define shwarn_arg(b, a, m) fprintf(stderr, "%s: %s: %s: %s\n", sh_argv0, b, a, m)

/* is_ something checks (some replacing ctypes functions) */

#define isalpha_(c) (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || (c) == '_')

#define isdigit_(c)  ((c) >= '0' && (c) <= '9')

#define isalnum_(c)  (isalpha_(c) || ((c) >= '0' && (c) <= '9'))

/**  check if char is operator  */
#define is_operator(c) \
  (c == '&' || c == '|' || c == ';' || c == '(' || c == ')' || c == '{' || \
   c == '}' || c == '<' || c == '>')

/**  check if char is line end  */
#define is_cmd_end(c) ((c == ' ') | (c == '\t') | (c == '\n'))

/* project specific replacements */

/** null terminated memcpy */
#define nmemcpy(d,s,l) memcpy(d, s, l), d[l] = '\0'

static inline int
atoi_(const char *s)
{
    int n = 0;
    while (isdigit_(*s))
        n = n * 10 + (*s++ - '0');
    return n;
}

/**  replicate strchrnul  */
static inline char *
strchrnul_(const char *s, int c)
{
  while (*s && *s != c)
    s++;
  return (char *)s;
}

/**  strndup using memcpy  */
static inline char *
strndup_(const char *s, size_t n)
{
  char *dup;
  
  if ((dup = malloc(n + 1))) {
    nmemcpy(dup, s, n);
  }
  return dup;
}

/** strcat using memcpy */
static inline char *
strcat_(char *dest, const char *src)
{
  size_t destlen = strlen(dest), srclen = strlen(src) + 1;
  memcpy(dest + destlen, src, srclen);
  return dest;
}

/** mempcpy implementation */
__attribute__((always_inline)) static inline char *
mempcpy_(char *dest, const char *src, size_t n)
{
  memcpy(dest, src, n);
  return dest + n;
}

/**  strdup using memcpy  */
static inline char *
strdup_(const char *s)
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

/* various small helpers */

/**  get length of char* array  */
static inline size_t
array_len(char **arr)
{
  size_t n = 0;
  while (arr[n])
    n++;
  return n;
}

/** join char** array into single string */
static inline char *
join_strn(char **arr, size_t t)
{
  char *p, *buf;
  size_t ac;

  if (!arr)
    return NULL;

  ac = array_len(arr);
  buf = st_alloc(t + ac);
  p = buf;

  for (char **a = arr; *a; a++) {
    size_t len;
    if (a != arr)
      *p++ = ' ';
    len = strlen(*a);
    memcpy(p, *a, len);
    p += len;
  }
  *p = '\0';
  return buf;
}

/**  skip whitespace  */
static inline const char *
skip_ws(const char *s)
{
  while (isspace(*s))
    s++;
  return s;
}

/**  brief find name and value from name=value pair  */
static inline void
read_assn(const char *assn, char **name, char **value)
{
  char *eq;
  size_t l;
  eq = strchrnul_(assn, '=');
  if (!eq) {
    *name = strdup_(assn);
    *value = NULL;
  } else {
    l = eq - assn;
    *name = strndup_(assn, l);
    *value = strdup_(eq + 1);
  }
}

static inline void
st_read_assn(const char *assn, char **name, char **value)
{
  char *eq;
  size_t l;
  eq = (char *)strchr(assn, '=');
  if (!eq) {
    /* st_strdup expanded */
    *name = st_strndup(assn, strlen(assn));
    *value = NULL;
  } else {
    l = eq - assn;
    *name = st_strndup(assn, l);
    /* st_strdup expanded */
    *value = st_strndup(eq + 1, strlen(eq + 1));
  }
}

/**  hash string for hash table  */
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

/**  hash string with known length  */
static inline unsigned int
hash_n(const char *s, size_t n, unsigned int buckets) {
  unsigned long h = 525201411107845655ull;
  for (size_t i = 0; i < n; i++) {
    h ^= (unsigned char)s[i];
    h *= 0x5bd1e9955bd1e995;
    h ^= h >> 47;
  }
  return h % buckets;
}

/* quote string handling shell escaping */
static inline char *
quotestrn(const char *s)
{
  size_t c;
  char *buf, *p;

  c = 3;
  for (size_t i = 0; s[i]; i++) {
    if (s[i] == '\'')
      c += 5;
    else
      c++;
  }
  buf = st_alloc(c);

  p = buf;
  *p++ = '\'';
  for (size_t i = 0; s[i]; i++) {
    if (s[i] == '\'') {
      *p++ = '\'';
      *p++ = '"';
      *p++ = '\'';
      *p++ = '"';
      *p++ = '\'';
    } else {
      *p++ = s[i];
    }
  }
  *p++ = '\'';
  *p++ = '\0';
  return buf;
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

#endif /* !UTILS_H */
