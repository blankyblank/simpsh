/* util.h - misc helper functions */
#ifndef UTILS_H
#define UTILS_H
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#include "alloc.h"
#include "main.h"

/* is_ something checks (some replacing ctypes functions) */

#define isalpha_(c)      (((c) >= 'a' && (c) <= 'z') || ((c) >= 'A' && (c) <= 'Z') || (c) == '_')
#define isdigit_(c)      ((c) >= '0' && (c) <= '9')
#define isalnum_(c)      (isalpha_(c) || ((c) >= '0' && (c) <= '9'))
#define is_ws(c)         (c == ' ' || c == '\t' || c == '\n')
#define is_ifs_nws(c, s) (!is_ws(c) && strchr(s, c))
#define arsz(a, o)       (sizeof(a) / sizeof(o))
/**  check if char is line end  */
#define is_cmd_end(c)    ((c == ' ') | (c == '\t') | (c == '\n'))
#define nts(s, l)        (s[l] = '\0')
#define nmemcpy(d, s, l) memcpy((d), (s), (l)), (d)[l] = '\0'
/**  check if char is operator  */
#define is_operator(c) \
  (c == '&' || c == '|' || c == ';' || c == '(' || c == ')' || c == '{' || \
   c == '}' || c == '<' || c == '>')

/* project specific replacements */

static inline int
atoi_(const char *s)
{
    int n = 0;
    while (isdigit_(*s))
        n = n * 10 + (*s++ - '0');
    return n;
}

static inline i64
atoll_(const char *restrict s, i64 *restrict res)
{
  i64 n = 0;
  int neg = 0;

  if (*s == '-') {
    neg = 1;
    s++;
  }

  if (!isdigit_(*s)) {
    *res = 0;
    return -1;
  }
  while (isdigit_(*s))
    n = n * 10 + (*s++ - '0');
  *res = neg ? -n : n;
  return 0;
}

/* convert long long to string  */
static inline size_t
lltoa(i64 val, char *buf)
{
  char *p, *start, *end;
  i64 uval;

  p = buf;

  if (val < 0) {
    *p++ = '-';
    uval = -val;
  } else {
    uval = val;
  }
  start = p;
  do {
    *p++ = '0' + (uval % 10);
    uval /= 10;
  } while (uval);
  end = p - 1;
  while (start < end) {
    char tmp;
    tmp = *start;
    *start++ = *end;
    *end-- = tmp;
  }
  *p = '\0';
  return p - buf;
}

/* convert int to string  */
static inline size_t
itoa(int val, char *buf)
{
  char *p, *start, *end;
  unsigned int uval;

  p = buf;

  if (val < 0) {
    *p++ = '-';
    uval = -val;
  } else {
    uval = val;
  }
  start = p;
  do {
    *p++ = '0' + (uval % 10);
    uval /= 10;
  } while (uval);
  end = p - 1;
  while (start < end) {
    char tmp;
    tmp = *start;
    *start++ = *end;
    *end-- = tmp;
  }
  *p = '\0';
  return p - buf;
}
/**  replicate strchrnul  */
static inline char *
strchrnul_(const char *s, int c)
{
  while (*s && *s != c)
    s++;
  return (char *)s;
}

/** strcat using memcpy */
static inline char *
strcat_(char *restrict dest, const char *restrict src)
{
  size_t destlen = strlen(dest), srclen = strlen(src) + 1;
  memcpy(dest + destlen, src, srclen);
  return dest;
}

/** mempcpy implementation */
__attribute__((always_inline)) static inline char *
mempcpy_(char *restrict dest, const char *restrict src, size_t n)
{
  memcpy(dest, src, n);
  return dest + n;
}

/* various small helpers */

/**  get length of char* array  */
#define array_len(a, c) \
  while ((a)[(c)]) \
    (c)++;

/** join char** array into single string */
static inline char *
join_strn(char **arr, size_t *t)
{
  char *p, *buf;
  size_t ac = 0, flen;
  size_t tlen = 0;

  if (!arr)
    return NULL;

  array_len(arr, ac);

  if (!t)
    for (size_t i = 0; i < ac; i++)
      tlen += strlen(arr[i]);
  else
    tlen = *t;

  buf = st_alloc(tlen + ac);
  p = buf;

  flen = 0;
  for (char **a = arr; *a; a++) {
    size_t len;
    if (a != arr)
      *p++ = ' ';
    len = strlen(*a);
    memcpy(p, *a, len);
     p += len, flen += len + 1;
  }
  *p = '\0';
  if (t)
    *t = flen;
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
read_assn(const char *assn, char **restrict name, char **restrict value)
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
st_read_assn(const char *assn, char **restrict name, char **restrict value)
{
  char *eq;
  size_t l, total;
  eq = (char *)strchr(assn, '=');
  total = strlen(assn);
  if (!eq) {
    /* st_strdup expanded */
    *name = st_strndup(assn, total);
    *value = NULL;
  } else {
    l = eq - assn;
    *name = st_strndup(assn, l);
    /* st_strdup expanded */
    *value = st_strndup(eq + 1, total - l - 1);
  }
}

/**  hash string for hash table  */
NO_UBSAN
static inline unsigned int
hash(const char *s, unsigned int buckets)
{
  u64 h = 525201411107845655ull;
  while (*s) {
    h ^= (unsigned char)*s;
    h *= 0x5bd1e9955bd1e995;
    h ^= h >> 47;
    s++;
  }
  return h & (buckets - 1);
}  // __attribute__((no_sanitize("unsigned-integer-overflow")))

/**  hash string with known length  */
NO_UBSAN
static inline unsigned int
hash_n(const char *s, size_t n, unsigned int buckets)
{
  u64 h = 525201411107845655ull;
  for (size_t i = 0; i < n; i++) {
    h ^= (unsigned char)s[i];
    h *= 0x5bd1e9955bd1e995;
    h ^= h >> 47;
  }
  return h & (buckets - 1);
}  // __attribute__((no_sanitize("unsigned-integer-overflow")))

/* quote string handling shell escaping */
static inline char *
quotestrn(const char *s)
{
  if (!s)
    return st_strdup("''");

  size_t c;
  char *buf, *p;

  c = 3;
  for (size_t i = 0; s[i]; i++) {
    if (s[i] == '\'')
      c += 5;
    else
      c++;
  }
  if (!(buf = st_alloc(c))) {
    perror("stalloc failed");
    return NULL;
  }

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
      slfree(args[i]);
      i++;
    }
    slfree(args);
  }
}

#endif /* !UTILS_H */
