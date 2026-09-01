#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include <sys/stat.h>

#include "glob.h"
#include "alloc.h"
#include "errmsg.h"
#include "simd.h"

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

  if ((*p == '*' && p[1] == '\0') || (strcmp(p, s) == 0 || (!*p && !*s)))
    return 1;

retry:
  while (*p) {
    switch (*p) {
      case '*':
        sp = p;
        ss = s;
        p++;
        continue;
      case '?':
        if (!*s || (pfl && (!*s || *s == '/')))
          goto backtrack;
        p++, s++;
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
      case '\\':
        if (p[1] && *s == p[1]) {
          p += 2;
          s++;
          continue;
        }
        if (p[1] && *s == '\\') {
          p++, s++;
          continue;
        }
        goto backtrack;
      default:
        if (*p == *s) {
          p++, s++;
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
    if (ss >= send)
      return 0;
    p = sp + 1;
    s = ++ss;
    goto retry;
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
  size_t lsep, len, dlen;
  size_t cnt;
  int sep, pfl, rd;
  const char *p;
  char *dir;
  DIR *d;
  struct dirent *f;

  lsep = sep = pfl = dlen = rd = 0;
  len = strlen(pattern);
  *result = NULL;

  if (len > 0 && pattern[len - 1] == '/') {
    rd = 1;
    len--;
  }

  {
    size_t i;
    for (i = 0; i < len; i++) {
      if (pattern[i] == '/') {
        lsep = i;
        sep = 1;
      }
    }
  }

  if (sep) {
    p = pattern + lsep + 1;
    dir = st_strndup(pattern, lsep + 1);
    dlen = lsep + 1;
  } else {
    p = pattern;
    dir = ".";
  }
  {
    size_t llen;
    char *lp;
    llen = &pattern[len] - p;
    lp = st_alloc(llen + 1);
    memcpy(lp, p, llen);
    lp[llen] = '\0';
    p = lp;
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
    if (rd) {
      struct stat sb;
      size_t o, tlen, flen;
      char tmp[PATH_MAX];

      flen = strlen(f->d_name);
      tlen = dlen + flen + 2;
      if (tlen >= sizeof(tmp))
        continue;
      if (dlen)
        memcpy(tmp, dir, dlen);
      memcpy(tmp + dlen, f->d_name, flen);
      o = dlen + flen;
      tmp[o++] = '/';
      tmp[o] = '\0';
      if (stat(tmp, &sb) < 0 || !S_ISDIR(sb.st_mode))
        continue;
    }
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
    if (sep || rd) {
      size_t o, tlen;
      char *fpath;

      tlen = dlen + flen + (rd ? 2 : 1);
      fpath = st_alloc(tlen + 1);
      if (dlen)
        memcpy(fpath, dir, dlen);
      memcpy(fpath + dlen, f->d_name, flen);
      o = dlen + flen;
      if (rd) {
        struct stat sb;
        fpath[o++] = '/';
        fpath[o] = '\0';
        if (stat(fpath, &sb) < 0 || !S_ISDIR(sb.st_mode))
          continue;
      } else {
        fpath[o] = '\0';
      }
      (*result)[i++] = fpath;
    } else {
      (*result)[i++] = st_strndup(f->d_name, flen);
    }
  }
  (*result)[i] = NULL;
  if (closedir(d) < 0)
    return sherrx(0, "closedir");
  qsort(*result, i, sizeof(char *), cmp);
  return i;
}

