#include "config.h"
#if ENABLE_TR
  #define _POSIX_C_SOURCE 200809L
  #ifdef __SSE4_1__
    #include <smmintrin.h>
  #endif /* __SSE4_1__ */

  #include "arg.h"
  #include "errmsg.h"
  #include "simd.h"
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
#define DELETED 0x100
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

#ifdef __SSE2__
static inline void
trshift(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, int delta)
{
  sint vlo, vhi, vd;
  size_t i = 0;
  vlo = _mm_set1_epi8((char)(lo - 1));
  vhi = _mm_set1_epi8((char)(hi + 1));
  vd = _mm_set1_epi8((char)delta);
  for (; i + 16 <= n; i += 16) {
    sint x, inr, y;
    x = _mm_loadu_si128((const sint *)(in + i));
    inr = _mm_and_si128(_mm_cmpgt_epi8(x, vlo), _mm_cmplt_epi8(x, vhi));
    y = _mm_add_epi8(x, _mm_and_si128(inr, vd));
    _mm_storeu_si128((sint *)(out + i), y);
  }
  for (; i < n; i++) {
    unsigned char c = in[i];
    if (c >= lo && c <= hi)
      out[i] = (unsigned char)(c + delta);
    else
      out[i] = c;
  }
}

static inline void
trtarget(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, unsigned char t, int invert)
{
  sint vlo, vhi, vt;
  size_t i = 0;
  vlo = _mm_set1_epi8((char)(lo - 1));
  vhi = _mm_set1_epi8((char)(hi + 1));
  vt = _mm_set1_epi8((char)t);
  for (; i + 16 <= n; i += 16) {
    sint x, inr, y;
    x = _mm_loadu_si128((const sint *)(in + i));
    inr = _mm_and_si128(_mm_cmpgt_epi8(x, vlo), _mm_cmplt_epi8(x, vhi));
#ifdef __SSE4_1__
    if (!invert)
      y = _mm_blendv_epi8(x, vt, inr);
    else
      y = _mm_blendv_epi8(vt, x, inr);
#else
    if (!invert)
      y = _mm_or_si128(_mm_and_si128(inr, vt), _mm_andnot_si128(inr, x));
    else
      y = _mm_or_si128(_mm_and_si128(inr, x), _mm_andnot_si128(inr, vt));
#endif /* __SSE4_1__ */
    _mm_storeu_si128((sint *)(out + i), y);
  }
  for (; i < n; i++) {
    unsigned char c = in[i];
    if ((c >= lo && c <= hi) != (invert))
      out[i] = t;
    else
      out[i] = c;
  }
}

#else
static inline void
trshift(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, int delta)
{
  for (size_t i = 0; i < n; i++) {
    unsigned char c = in[i];
    if (c >= lo && c <= hi)
      out[i] = (unsigned char)(c + delta);
    else
      out[i] = c;
  }
}

static inline void
trtarget(unsigned char *in, unsigned char *out, size_t n,
    unsigned char lo, unsigned char hi, unsigned char t, int invert)
{
  for (size_t i = 0; i < n; i++) {
    unsigned char c = in[i];
    if ((c >= lo && c <= hi) != (invert))
      out[i] = t;
    else
      out[i] = c;
  }
}
#endif /* ifdef __SSE2__ */


static inline void
trput(unsigned char *arr, size_t *n, int c)
{
  if (*n < 256)
    arr[(*n)++] = c;
}

static inline int
iscontig(unsigned char *a, int n)
{
  for (int i = 1; i < n; i++)
    if (a[i] != a[0] + i)
      return 0;
  return 1;
}
int
trcmd(char *argv[])
{
  enum {
    TRANSLATE,
    SQUEEZE,
    SQUEEZE_TR,
    DELETE,
    DEL_SQUEEZE,
  };
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
  int n1, n2 = 0, mode = 0;
  unsigned char set1[256], set2[256];
  char *str1 = NULL, *str2 = NULL;

  if (flags & dfl)
    mode = (flags & sfl) ? DEL_SQUEEZE : DELETE;
  else if (flags & sfl)
    mode = (argc == 2) ? SQUEEZE_TR : SQUEEZE;
  else
    mode = TRANSLATE;
  min = max = (mode == DELETE || mode == SQUEEZE) ? 1 : 2;
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
    which = (mode == DEL_SQUEEZE) ? s2s : s2t;
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
  unsigned short trnslt[256];
  unsigned char sq[256] = { 0 };
  unsigned char lo = 0, hi = 0, t = 0;
  int fast = 0, delta = 0, invert = 0;
  size_t nsrc = 0;

  int bufsize = BUFSIZ;
  in = st_alloc(bufsize);
  out = st_alloc(bufsize);

  int last = 0x100;
  size_t r;

  if (mode == TRANSLATE && iscontig(set1, n1)) {
    lo = set1[0];
    hi = set1[n1 - 1];
    if (hi <= 126) {
      if (n2 == 1) {
        fast = 2;
        t = set2[0];
        invert = !!(flags & cfl);
      } else if (!(flags & cfl) && n2 >= n1 && iscontig(set2, n1)) {
        fast = 1;
        delta = set2[0] - set1[0];
      }
    }
  }

  if (!fast) {
    for (int i = 0; i < 256; i++)
      trnslt[i] = i;
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

    if (mode == TRANSLATE || mode == SQUEEZE_TR)
      for (size_t j = 0; j < nsrc; j++)
        trnslt[src[j]] = set2[j < (size_t)n2 ? j : (size_t)n2 - 1];
    if (mode == DELETE || mode == DEL_SQUEEZE)
      for (size_t j = 0; j < nsrc; j++)
        trnslt[src[j]] = DELETED;
    if (mode == SQUEEZE || mode == SQUEEZE_TR || mode == DEL_SQUEEZE) {
      if (mode == SQUEEZE)
        for (size_t j = 0; j < nsrc; j++)
          sq[src[j]] = 1;
      else
        for (size_t j = 0; j < (size_t)n2; j++)
          sq[set2[j]] = 1;
    }
  }

  int bigbuf = 0;
  while ((r = fread(in, 1, bufsize, shin)) > 0) {
    size_t n;
    if (bufsize != 65536 && bigbuf) {
      unsigned char *in2, *out2;
      bufsize = 65536;
      in2 = st_alloc(bufsize);
      out2 = st_alloc(bufsize);
      memcpy(in2, in, r);
      in = in2;
      out = out2;
    }
    if (fast == 1) {
      trshift(in, out, r, lo, hi, delta);
      n = r;
    } else if (fast == 2) {
      trtarget(in, out, r, lo, hi, t, invert);
      n = r;
    } else {
      n = 0;
      for (size_t i = 0; i < r; i++) {
        int c = trnslt[in[i]];
        if (c == DELETED)
          continue;
        if (sq[c] && c == last)
          continue;
        out[n++] = c;
        last = c;
      }
    }
    if (ferror(shin))
      return sherr(1, argv0, "Bad file descriptor");
    fwrite(out, 1, n, shout);
    bigbuf = 1;
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
