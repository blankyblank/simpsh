#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <dirent.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "glob.h"
#include "alloc.h"
#include "simd.h"
#include "utils.h"

#define GLOB_CAP 64

int
ismetachar(const char *str, size_t len)
{
  size_t pos = 0;
  while (pos < len) {
    char c;
    size_t s, e;

    size_t dlm = sscndelim(str + pos, len - pos, "*?[", 3);
    if (dlm >= len - pos) 
      break;

    c = str[pos + dlm];
    if (c == '*' || c == '?')
      return 1;

    s = pos + dlm + 1;
    if (s < len && str[s] == ']')
      s++;
    if (s < len && (str[s] == '!' || str[s] == '^'))
      s++;
    if (s < len) {
      e = sscndelim(str + s, len - s, "]", 1);
      if (e < len - s)
        return 1;
    }
    pos = pos + dlm + 1;
  }
  return 0;
}

static int
match_bracket(const char *p, char c, const char **end)
{
    int neg = 0;
    int matched = 0;

    if (*p == '!' || *p == '^') {
        neg = 1;
        p++;
    }

    if (*p == ']') {
        if (c == ']')
            matched = 1;
        p++;
    }

    while (*p && *p != ']') {
        if (p[1] == '-' && p[2] && p[2] != ']') {
            if (c >= *p && c <= p[2])
                matched = 1;
            p += 3;
        } else {
            if (c == *p)
                matched = 1;
            p++;
        }
    }

    if (*p == ']') {
        *end = p;
        return neg ? !matched : matched;
    }
    *end = NULL;
    return 0;
}

int
globmatch(const char *restrict p, const char *restrict s, int pfl)
{
  const char *sp = NULL, *ss = NULL;
  const char *send = s + strlen(s);

  if (*p == '*' && p[1] == '\0')
    return 1;
  else if (strcmp(p, s) == 0 || (!*p && !*s))
    return 1;

  while (*p) {
    switch (*p) {
      case '*':
        sp = p;
        ss = s;
        p++;
        continue;
      case '?':
        if (pfl && (!*s || *s == '/'))
          goto backtrack;
        p++;
        s++;
        continue;
      case '[':
        if (!*s)
          goto backtrack;
        {
          const char *bend;
          if (match_bracket(p + 1, *s, &bend)) {
            p = bend + 1;
            s++;
          } else {
            goto backtrack;
          }
        }
        continue;
      default:
        if (*p == *s) {
          p++;
          s++;
        } else {
backtrack:
          if (!sp || ss >= send)
            return 0;
          p = sp + 1;
          s = ++ss;
        }
    }
  }
  if (*s) {
    if (!sp)
      return 0;
    else
      for (const char *r = sp + 1; *r; r++)
        if (*r != '*')
          return 0;
  }
  return 1;
}

static inline int
cmp(const void *a, const void *b)
{
  return strcoll(*(const char **)a,*(const char **)b);
}

int
globexpand(const char *restrict pattern, char ***result)
{
  size_t lsep = 0, len = 0;
  size_t cnt;
  int sep = 0, pfl = 0;
  const char *p;
  char *dir;
  DIR *d;
  struct dirent *f;

  *result = NULL;
  for (; pattern[len]; len++) {
      if (pattern[len] == '/') {
        lsep = len;
        sep = 1;
      }
  }
  if (sep) {
    p = pattern + lsep + 1;
    dir = st_strndup(pattern, lsep);
  } else {
    p = pattern;
    dir = ".";
  }

  if (!(d = opendir(dir)))
    return 0;
  cnt = 0;
  while ((f = readdir(d))) {
    if ((*f->d_name == '.' && f->d_name[1] == '.' && f->d_name[2] == '\0') ||
        (*f->d_name == '.' && f->d_name[1] == '\0'))
      continue;
    if (p[0] != '.' && f->d_name[0] == '.')
      continue;
    if (!globmatch(p, f->d_name, pfl))
      continue;
    cnt++;
  }
  *result = st_alloc((cnt + 1) * sizeof(char *));
  rewinddir(d);

  size_t i = 0;
  while ((f = readdir(d))) {
    size_t flen;
    if ((*f->d_name == '.' && f->d_name[1] == '.' && f->d_name[2] == '\0') ||
        (*f->d_name == '.' && f->d_name[1] == '\0'))
      continue;
    if (p[0] != '.' && f->d_name[0] == '.')
      continue;
    if (!globmatch(p, f->d_name, pfl))
      continue;

    flen = strlen(f->d_name);
    if (sep) {
      size_t tlen = flen + lsep + 1;
      char *fpath = st_alloc(tlen + 1);
      memcpy(fpath, f->d_name, lsep);
      fpath[lsep] = '/';
      memcpy(fpath + lsep + 1, f->d_name, flen);
      fpath[flen] = '\0';

    } else {
      (*result)[i++] = st_strndup(f->d_name, flen);
    }
  }
  (*result)[i] = NULL;
  if (closedir(d) < 0)
    err(0, "closedir");
  qsort(*result, i, sizeof(char *), cmp);
  return i;
}

