#include "config.h"
#if ENABLE_TR
  #define _POSIX_C_SOURCE 200809L

  #include "arg.h"
  #include "errmsg.h"
  #include "utils.h"

enum {
  cfl = 1 << 0,
  dfl = 1 << 1,
  sfl = 1 << 2,
};

enum {
  s1,
  s2t,
  s2s,
};

static const struct {
  const char *name;
  unsigned char r[4][2];
} trclasses[] = {
  { "alnum",  { { '0', '9' },   { 'A', 'Z' },   { 'a', 'z' },   { 0, 0 } }       },
  { "alpha",  { { 'A', 'Z' },   { 'a', 'z' },   { 0, 0 },       { 0, 0 } }       },
  { "blank",  { { '\t', '\t' }, { ' ', ' ' },   { 0, 0 },       { 0, 0 } }       },
  { "cntrl",  { { 0x00, 0x1F }, { 0x7F, 0x7F }, { 0, 0 },       { 0, 0 } }       },
  { "digit",  { { '0', '9' },   { 0, 0 },       { 0, 0 },       { 0, 0 } }       },
  { "graph",  { { 0x21, 0x7E }, { 0, 0 },       { 0, 0 },       { 0, 0 } }       },
  { "lower",  { { 'a', 'z' },   { 0, 0 },       { 0, 0 },       { 0, 0 } }       },
  { "print",  { { 0x20, 0x7E }, { 0, 0 },       { 0, 0 },       { 0, 0 } }       },
  { "punct",  { { 0x21, 0x2F }, { 0x3A, 0x40 }, { 0x5B, 0x60 }, { 0x7B, 0x7E } } },
  { "space",  { { '\t', '\r' }, { ' ', ' ' },   { 0, 0 },       { 0, 0 } }       },
  { "upper",  { { 'A', 'Z' },   { 0, 0 },       { 0, 0 },       { 0, 0 } }       },
  { "xdigit", { { '0', '9' },   { 'A', 'F' },   { 'a', 'f' },   { 0, 0 } }       },
};

static int parsetrset(char *, unsigned char *, int);
static int parsetrunit(char **);

static inline void
trput(unsigned char *arr, size_t *n, int c)
{
  if (*n < 256)
    arr[(*n)++] = c;
}

int
trcmd(char *argv[])
{
  size_t argc = 0;
  int flags = 0;
  int which = 0;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'c':
    case 'C':
      flags |= cfl;
      break;
    case 'd':
      flags |= dfl;
      break;
    case 's':
      flags |= sfl;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  size_t min, max;
  int n1, n2 = 0;
  unsigned char set1[256], set2[256];
  char *str1 = NULL, *str2 = NULL;

  if (flags & dfl)
    min = max = (flags & sfl) ? 2 : 1;
  else if (flags & sfl)
    min = 1, max = 2;
  else
    min = max = 2;
  if (argc < min)
    return shwarn(argv0, "missing operand");
  if (argc > max)
    return shwarn(argv0, "extra operand");

  str1 = *argv++;
  if ((n1 = parsetrset(str1, set1, s1)) < 0)
    return 1;
  if (!n1)
    return shwarn(argv0, "empty string1");
  if (argc == 2) {
    which = (flags & dfl) ? s2s : s2t;
    str2 = *argv++;
    if ((n2 = parsetrset(str2, set2, which)) < 0)
      return 1;
    if (!n2)
      return shwarn(argv0, "empty string2");
    if (which == s2t &&
        (strstr(str2, "[:upper:]") || strstr(str2, "[:lower:]")) &&
        !(strstr(str1, "[:upper:]") || strstr(str1, "[:lower:]")))
      return shwarn(argv0, "string2 requires a match in string1");
  }

  unsigned char *in, *out;
  unsigned char in1[256] = {0}, src[256];
  unsigned char trtab[256], del[256] = { 0 }, sq[256] = { 0 };
  size_t nsrc = 0;

  int bufsize = BUFSIZ; // what is the memory page size? probably should make it that
  in = st_alloc(bufsize);
  out = st_alloc(bufsize);

  int last = 0x100;
  size_t r;

  for (int i = 0; i < 256; i++)
    trtab[i] = i;
  for (int i = 0; i < n1; i++) {
    in1[set1[i]] = 1;
  }
  if (flags & cfl) {
    for (int i = 0; i < 256; i++)
      if (!in1[i])
        src[nsrc++] = i;
  } else {
    memcpy(src, set1, n1);
    nsrc = n1;
  }

  if (argc == 2 && !(flags & dfl))
    for (size_t j = 0; j < nsrc; j++)
      trtab[src[j]] = set2[j < (size_t)n2 ? j : (size_t)n2 - 1];
  if (flags & dfl)
    for (size_t j = 0; j < nsrc; j++)
      del[src[j]] = 1;
  if (flags & sfl) {
    if (argc == 1) 
      for (size_t j = 0; j < nsrc; j++)
        sq[src[j]] = 1;
    else
      for (size_t j = 0; j < (size_t)n2; j++)
        sq[set2[j]] = 1;
  }

  while ((r = fread(in, 1, bufsize, shin)) > 0) {
    size_t n = 0;
    for (size_t i = 0; i < r; i++) {
      int c = in[i];
      if (del[c])
        continue;
      c = trtab[c];
      if (sq[c] && c == last)
        continue;
      out[n++] = c;
      last = c;
    }
    if (ferror(shin)) {
      return sherr(1, argv0, "Bad file descriptor");
    }
    fwrite(out, 1, n, shout);
  }

  return 0;
}

static int
parsetrset(char *s, unsigned char *arr, int which)
{
  size_t n = 0;
  for (char *p = s; *p;) {
    int c, d;
    size_t len;

    if (*p == '[') {
      if (p[1] == ':') {
        char *end;
        if (!(end = strstr(p + 2, ":]"))) {
          shwarn("tr", "unterminated character class");
          return -1;
        }
        len = pntlen((p + 2), end);
        size_t j;
        for (j = 0; j < arsz(trclasses); j++)
          if (strlen(trclasses[j].name) == len &&
              !memcmp(trclasses[j].name, p + 2, len))
            break;
        if (j == arsz(trclasses)) {
          shwarn("tr", "unknown character class");
          return -1;
        }
        if (which == s2t &&
            memcmp(trclasses[j].name, "lower", sizeof("lower") - 1) &&
            memcmp(trclasses[j].name, "upper", sizeof("upper") - 1)) {
          shwarn("tr", "character class not valid in string2");
          return -1;
        }
        for (int ind = 0; ind < (int)arsz(trclasses[j].r) &&
                          (trclasses[j].r[ind][0] || trclasses[j].r[ind][1]); ind++)
          for (int k = trclasses[j].r[ind][0]; k <= trclasses[j].r[ind][1]; k++)
            trput(arr, &n, k);
        p = end + 2;
        continue;
      } else if (p[1] == '=') {
        char *q = p + 2;
        if (!*q) {
          shwarn("tr", "invalid equivalence class");
          return -1;
        }
        if ((c = parsetrunit(&q)) < 0)
          return -1;
        if (*q != '=' || q[1] != ']') {
          shwarn("tr", "invalid equivalence class");
          return -1;
        }
        trput(arr, &n, c);
        p = q + 2;
        continue;
      } else {
        char *q = p + 1;
        size_t cnt = 0;
        if (!*q) {
          trput(arr, &n, *p++);
          continue;
        }
        if ((c = parsetrunit(&q)) < 0)
          return -1;
        if (*q != '*') {
          trput(arr, &n, *p++);
          continue;
        }
        if (which == s1) {
          shwarn("tr", "[c*n] only valid in string2");
          return -1;
        }
        q++;
        while (*q >= '0' && *q <= '9') {
          if (cnt <= 256)
            cnt = cnt * 10 + (*q - '0');
          q++;
        }
        if (*q != ']') {
          shwarn("tr", "malformed repeat");
          return -1;
        }
        cnt = (cnt) ? cnt : 1;
        cnt = (cnt < 256) ? cnt : 256;
        for (size_t i = 0; i < cnt; i++)
          trput(arr, &n, c);
        p = q + 1;
        continue;
      }
    }
    if ((c = parsetrunit(&p)) < 0)
      return -1;
    if (*p != '-' || p[1] == '\0') {
      trput(arr, &n, c);
    } else if (p[1] == '[') {
      shwarn("tr", "invalid range endpoint");
      return -1;
    } else {
      p++;
      if ((d = parsetrunit(&p)) < 0)
        return -1;
      if (c > d) {
        shwarn("tr", "reversed range");
        return -1;
      }
      for (int i = c; i <= d; i++)
        trput(arr, &n, i);
    }
  }
  return n;
}

static int parsetrunit(char **p)
{
  if ((unsigned char)**p != '\\')
    return (unsigned char)*(*p)++;
  (*p)++;
  switch (**p) {
    case 'a':
      (*p)++;
      return '\a';
    case 'b':
      (*p)++;
      return '\b';
    case 'f':
      (*p)++;
      return '\f';
    case 'n':
      (*p)++;
      return '\n';
    case 'r':
      (*p)++;
      return '\r';
    case 't':
      (*p)++;
      return '\t';
    case 'v':
      (*p)++;
      return '\v';
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      {
        int v = 0, cnt = 0;
        while (cnt < 3 && (**p >= '0' && **p <= '7')) {
          v = v * 8 + (*(*p)++ - '0');
          cnt++;
        }
        if (v > 255) {
          shwarn("tr", "octal value out of range");
          return -1;
        }
        return v;
      }
    case '\0':
      shwarn("tr", "ends with unescaped backslash");
      return -1;
    case '\\':
    default:
      return *(*p)++;
  }
}

#endif /* ENABLE_TR */


















