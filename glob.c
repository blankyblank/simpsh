#include <stdlib.h>
#define _POSIX_C_SOURCE 200809L
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

    size_t dlm = simd_scan_delim(str + pos, len - pos, "*?[", 3);
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
      e = simd_scan_delim(str + s, len - s, "]", 1);
      if (e < len - s)
        return 1;
    }
    pos = pos + dlm + 1;
  }
  return 0;
}

int
globmatch(const char *restrict p, const char *restrict s)
{
  const char *sp, *ss;

  if (*p == '*' && p[1] == '\0') {
    return 1;
  } else if (strcmp(p, s) == 0) {
    return 1;
  } else if (!*p && !*s) {
    return 1;
  }
  sp = NULL;
  ss = NULL;
  while (*p) {
    switch (*p) {
      case '*':
        if (!sp) {
          sp = p;
          ss = s;
        }
        p++;
        continue;
      case '?':
        if (*s && *s != '/') {
          if (!sp) {
            sp = p;
            ss = s;
          }
          p++;
          s++;
        } else if (sp) {
          s = ++ss;
          p = sp + 1;
        } else {
          return 0;
        }
        continue;
      default:
        if (*p == *s) {
          p++;
          s++;
        } else {
          if (ss && sp) {
            p = sp + 1;
            s = ss + 1;
            ss = NULL;
            sp = NULL;
            continue;
          } else {
            return 0;
          }
        }
        break;
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
  size_t cnt, cap = GLOB_CAP;
  int sep = 0;
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
  *result = st_alloc(GLOB_CAP * sizeof(char *));
  cnt = 0;
  while ((f = readdir(d))) {
    size_t flen, tlen = 0;
    if ((*f->d_name == '.' && f->d_name[1] == '.' && f->d_name[2] == '\0') ||
        (*f->d_name == '.' && f->d_name[1] == '\0'))
      continue;
    if (p[0] != '.' && f->d_name[0] == '.')
      continue;
    if (!globmatch(p, f->d_name))
      continue;

    if (cnt >= cap) {
      char **nres = st_alloc((cap * 2) * sizeof(char *));
      memcpy(nres, *result, cnt * sizeof(char *));
      cap *= 2;
      *result = nres;
    }
    flen = strlen(f->d_name);
    if (sep) {
      // tlen = flen + lsep + 1;
      st_write(pattern, lsep, tlen);
      st_putc('/');
      st_write(f->d_name, flen, tlen);
    } else {
      st_write(f->d_name, flen, tlen);
    }
    (*result)[cnt++] = grab_str(tlen);
  }
  (*result)[cnt] = NULL;
  if (closedir(d) < 0)
    err(0, "closedir");
  qsort(*result, cnt, sizeof(char *), cmp);
  return cnt;
}

