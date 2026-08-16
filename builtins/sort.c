#include "config.h"
#if ENABLE_SORT
#define _POSIX_C_SOURCE 200809L

#include "arg.h"
#include "errmsg.h"
#include "lineio.h"
#include "simd.h"
#include "utils.h"

#define KEYCAP 16
#define LINECAP 128

typedef struct {
  int startcol;
  int endcol;
  int startchr;
  int endchr;
  int flags;
  int own;
} keydef;

typedef struct {
  char *line;
  size_t llen;
  char *src;
  int lineno;
} ln;

static keydef keys[KEYCAP];

enum {
  dict = 1 << 0,
  num = 1 << 1,
  icase = 1 << 2,
  inprnt = 1 << 3,
  rev = 1 << 4,
  strtb = 1 << 5,
  endb = 1 << 6,
  ver = 1 << 7,
};

enum {
  cfl = 1 << 0,
  Cfl = 1 << 1,
  mfl = 1 << 2,
  ofl = 1 << 3,
  ufl = 1 << 4,
};

static size_t nkeyd;
static char *outfile;
static char sep;
static int hassep;
static int hasout;
static unsigned flags;
static unsigned mode;

static int bytecmp(const char *a, size_t la, const char *b, size_t lb);
static int parsekey(char *s, keydef *kd);
static int frange(const char *, size_t , const keydef *, const char **, const char **);
static int keycmp(const char *, size_t , const char *, size_t, int);
static ln *sortmerge(const ln *, size_t, const ln *, size_t, size_t *);
static int vcmpc(int, int);
static int vcmpv(const char *, size_t, const char *, size_t);
static size_t vsfx(const char *, const char *);
static int vercmp(const char *, size_t, const char *, size_t);

static inline int
sortcmp(const void *a, const void *b)
{
  return keycmp(((const ln *)a)->line, ((const ln *)a)->llen,
                ((const ln *)b)->line, ((const ln *)b)->llen, 1);
}

int
sortcmd(char *argv[])
{
  size_t argc = 0;
  int status = 0;
  char *kdstr = NULL;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'c':
      mode |= cfl;
      break;
    case 'C':
      mode |= Cfl;
      break;
    case 'm':
      mode |= mfl;
      break;
    case 'u':
      mode |= ufl;
      break;
    case 'b':
      flags |= strtb|endb;
      break;
    case 'd':
      flags |= dict;
      break;
    case 'f':
      flags |= icase;
      break;
    case 'i':
      flags |= inprnt;
      break;
    case 'n':
      flags |= num;
      break;
    case 'r':
      flags |= rev;
      break;
    case 'V':
      flags |= ver;
      break;
    case 'k':
        if (!(kdstr = EARGF(no_opt(argv0, ARGC()))))
          return 1;
        if (nkeyd >= KEYCAP) {
          return usage(argv0, helpmsgs[SORTH].usage), 1;
        }
        if (parsekey(kdstr, &keys[nkeyd++]) < 0)
          return shwarn_arg(argv0, kdstr, "bad key");
        break;
    case 'o':
      if (!(outfile = EARGF(no_opt(argv0, ARGC()))))
        return 1;
      hasout = 1;
      break;
    case 't':
      if (!(sep = *EARGF(no_opt(argv0, ARGC()))))
        return 1;
      hassep = 1;
      break;
    default:
      return bad_opt(argv0, ARGC());
  }
  ARGEND

  if (argc > 1 && (mode & (cfl | Cfl)))
    return usage(argv0, helpmsgs[SORTH].usage), 1;

  int res;
  size_t linec = 0, rlen = 0;
  size_t nsrc, linecap = LINECAP;;
  ln *lines = NULL, *run = NULL, *mrun = NULL;

  nsrc = (argc) ? argc : 1;
  if (!(mode & mfl))
    lines = st_alloc(linecap * sizeof(ln));

  for (size_t i = 0; i < nsrc; i++) {
    FILE *fp;
    lr_t lr;
    size_t llen = 0;
    char *path, *arg;
    int lnno = 1;

    arg = argv[i];
    path = (argc && !(arg[0] == '-' && arg[1] == '\0')) ? argv[i] : NULL;
    if (!(fp = lropen(&lr, path))) {
      status = sherr(1, argv0, path ? path : "(stdin)");
      continue;
    }

    if (mode & mfl) {
      ln *flns;
      size_t flen = 0, mlen;

      flns = st_alloc(linecap * sizeof(ln));
      while ((flns[flen].line = lrread(&lr, &llen))) {
        flns[flen].llen = llen;
        flns[flen].lineno = lnno++;
        flns[flen++].src = path ? path : "(stdin)";
        if (flen >= linecap) {
          linecap *= 2;
          streallocar(flns, linecap, flen, ln);
        }
      }
      if (!run) {
        run = flns;
        rlen = flen;
      } else {
        mrun = sortmerge(run, rlen, flns, flen, &mlen);
        run = mrun, rlen = mlen;
      }
    } else {
      if (linec >= linecap) {
        linecap *= 2;
        streallocar(lines, linecap, linec, ln);
      }
      while ((lines[linec].line = lrread(&lr, &llen))) {
        lines[linec].llen = llen;
        lines[linec].lineno = lnno++;
        lines[linec++].src = path ? path : "(stdin)";
        if (linec >= linecap) {
          linecap *= 2;
          streallocar(lines, linecap, linec, ln);
        }
      }
    }
    if (fp != shin)
      fclose(fp);
  }

  if (mode & mfl)
    lines = run, linec = rlen;

  if (mode & (cfl | Cfl)) {
    for (size_t i = 1; i < linec; i++) {
      res = keycmp(lines[i - 1].line, lines[i - 1].llen, lines[i].line, lines[i].llen,
          (mode & ufl) ? 0 : 1);
      if (res > 0) {
        if (mode & cfl)
          fprintf(stderr, "%s: %s:%d: disorder: %s\n", argv0, lines[i].src,
                  lines[i].lineno, lines[i].line);
        return 1;
      }
      if (mode & ufl) {
        if (!res) {
          fprintf(stderr, "%s: %s:%d: disorder: %s\n", argv0, lines[i].src,
                  lines[i].lineno, lines[i].line);
          return 1;
        }
      }
    }
    return 0;
  }

  if (!(mode & mfl))
    qsort(lines, linec, sizeof(ln), sortcmp);

  FILE *of = (hasout) ? fopen(outfile, "w") : shout;
  if (!of)
    return sherr(1, argv0, (hasout) ? outfile : "(stdout)");
  for (size_t i = 0; i < linec; i++) {
    if ((mode & ufl) && i > 0) {
      if (!(res = keycmp(lines[i - 1].line, lines[i - 1].llen, lines[i].line, lines[i].llen, 0)))
        continue;
    }
    fwrite(lines[i].line, 1, lines[i].llen, of);
    fputc('\n', of);
  }
  if (of != shout)
    fclose(of);

  return status;
}

static int
parsekey(char *s, keydef *kd)
{
  *kd = (keydef) { 0 };
  kd->startcol = 1;

  if ((kd->startcol = (int)strtol(s, &s, 10)) < 1)
    return -1;
  if (*s == '.') {
    s++;
    if ((kd->startchr = (int)strtol(s, &s, 10)) < 1)
      return -1;
  }
  while (isalpha_((unsigned char)*s)) {
    switch (*s) {
      case 'b':
        kd->flags |= strtb;
        break;
      case 'd':
        kd->flags |= dict;
        break;
      case 'f':
        kd->flags |= icase;
        break;
      case 'i':
        kd->flags |= inprnt;
        break;
      case 'n':
        kd->flags |= num;
        break;
      case 'r':
        kd->flags |= rev;
        break;
      case 'V':
        kd->flags |= ver;
        break;
      default:
        return -1;
    }
    s++;
  }
  if (*s == ',') {
    s++;
    if ((kd->endcol = (int)strtol(s, &s, 10)) < 0)
      return -1;
    if (*s == '.') {
      s++;
      if ((kd->endchr = (int)strtol(s, &s, 10)) < 0)
        return -1;
    }
    while (isalpha_((unsigned char)*s)) {
      switch (*s) {
        case 'b':
          kd->flags |= endb;
          break;
        case 'd':
          kd->flags |= dict;
          break;
        case 'f':
          kd->flags |= icase;
          break;
        case 'i':
          kd->flags |= inprnt;
          break;
        case 'n':
          kd->flags |= num;
          break;
        case 'r':
          kd->flags |= rev;
          break;
        case 'V':
          kd->flags |= ver;
          break;
        default:
          return -1;
      }
      s++;
    }
  }
  if (*s != '\0')
    return -1;
  kd->own = (kd->flags != 0);
  return 0;
}


static ln *
sortmerge(const ln *a, size_t na, const ln *b, size_t nb, size_t *ol)
{
  size_t i, j;
  int res, cnt = 0;
  ln *l;

  l = st_alloc((na + nb) * sizeof(ln));

  for (i = j = 0; i < na && j < nb ; ) {
    res = keycmp(a[i].line, a[i].llen, b[j].line, b[j].llen, 1);
    if (res <= 0)
      l[cnt++] = a[i++];
    else
      l[cnt++] = b[j++];
  }
  if (i >= na && j != nb)
    for (;j < nb; j++)
      l[cnt++] = b[j]; 
  else if (i != na && j >= nb)
    for (; i < na; i++)
      l[cnt++] = a[i];

  *ol = na + nb;
  return l;
}

static inline const char *
nextfield(const char *p, const char *end)
{
  int off;

  if (p == end)
    return end;
  if (hassep) {
    off = memchr_(p, pntlen(p, end), sep);
    if (p + off + 1 > end)
      return end;
    return p + off + 1;
  }
  for (; p < end && (*p == ' ' || *p == '\t'); p++);
  for (; p < end && *p != ' ' && *p != '\t'; p++);
  return p;
}

static inline const char *
fieldend(const char *p, const char *end)
{
  int off;
  if (hassep) {
    off = memchr_(p, pntlen(p, end), sep);
    return p + off;
  }
  for (; p < end && (*p == ' ' || *p == '\t'); p++);
  for (; p < end && *p != ' ' && *p != '\t'; p++);
  return p;
}

static int
frange(const char *line, size_t len,
       const keydef *kd, const char **strto, const char **endo)
{
  const char *p, *end = NULL;
  const char *fstart = NULL, *fend = NULL;
  const char *strtp = NULL, *endp;
  int eff;

  p = line;
  end = line + len;
  eff = (kd->own) ? kd->flags : (int)flags;
  if (kd->startcol == 1) {
    p = line;
    fstart = line;
  } else if (kd->startcol > 1) {
    for (int n = 1; n < kd->startcol; n++)
      if ((p = nextfield(p, end)) >= end)
        return 1;
    fstart = p;
  }
  fend = fieldend(p, end);

  if (eff & strtb) {
    fstart += sskipspace(fstart, pntlen(fstart, fend));
  }
  if (kd->startchr >= 1) {
    strtp = fstart + (kd->startchr - 1);
  } else {
    strtp = fstart;
  }
  if (strtp > fend)
    strtp = fend;
  *strto = strtp;

  const char *efstrt = NULL, *efend = NULL;

  if (!kd->endcol) {
    *endo = end;
  } else {
    efstrt = line;
    for ( int n = 1 ; n < kd->endcol; n++) {
      if ((efstrt = nextfield(efstrt, end)) >= end) {
        efstrt = end;
        break;
      }
    }
    efend = fieldend(efstrt, end);
    if (eff & endb)
      efstrt += sskipspace(efstrt, pntlen(efstrt, efend));
    endp = (kd->endchr >= 1) ? efstrt + kd->endchr : efend;
    if (endp > efend)
      endp = efend;
    *endo = endp;
  }

  if (*strto >= *endo)
    return 1;
  return 0;
}

static int
numcmp(const char *a, size_t la, const char *b, size_t lb)
{
  // TODO: I'm really not happy with the structure of this.
  // gotta find a better way.
  const char *aend, *bend;
  int aneg = 0, bneg = 0;
  long double aval = 0, bval = 0;

  aend = a + la;
  bend = b + lb;
  for (;a < aend && (*a == ' ' || *a == '\t'); a++);
  for (;b < bend && (*b == ' ' || *b == '\t'); b++);
  if (a < aend) {
    if (*a == '+')
      a++;
    if (*a == '-') {
      aneg = 1;
      a++;
    }
  }
  if (b < bend) {
    if (*b == '+')
      b++;
    if (*b == '-') {
      bneg = 1;
      b++;
    }
  }

  long double adec = 0, bdec = 0;
  int dot = 0, denom = 10;

  if (a < aend) {
    for (; a < aend && (isdigit_(*a) || *a == '.'); a++) {
      if (*a == '.') {
        dot = 1;
        continue;
      }
      if (dot) {
        denom *= 10;
        adec = adec * 10 + (*a - '0');
        continue;
      }
      aval = aval * 10 + (*a - '0');
    }
    aval += adec / denom;
    aval = aneg ? -aval : aval;
    dot = 0, denom = 10;
  }
  if (b < bend) {
    for (; b < bend && (isdigit_(*b) || *b == '.'); b++) {
      if (*b == '.') {
        dot = 1;
        continue;
      }
      if (dot) {
        denom *= 10;
        bdec = bdec * 10 + (*b - '0');
        continue;
      }
      bval = bval * 10 + (*b - '0');
    }
    bval += bdec / denom;
    bval = bneg ? -bval : bval;
  }

  if (aval < bval)
    return -1;
  if (aval > bval)
    return 1;
  return 0;
}

static int
dictcmp(const char *a, size_t la, const char *b, size_t lb, int flg)
{
  const char *aend, *bend;
  char ac, bc;
  aend = a + la;
  bend = b + lb;

  for (; a < aend && b < bend; a++, b++) {
    if (flg & inprnt) {
      while (a < aend && !isprint(*a))
        a++;
      while (b < bend && !isprint(*b))
        b++;
    }
    if (flg & dict) {
      while (a < aend && !isalnum_(*a) && *a != ' ')
        a++;
      while (b < bend && !isalnum_(*b) && *b != ' ')
        b++;
    }
    if (a == aend || b == bend)
      break;
    if (flg & icase) {
      ac = tolower((unsigned char)*a);
      bc = tolower((unsigned char)*b);
    } else {
      ac = *a;
      bc = *b;
    }
    if (ac != bc)
      return (int)ac - bc;
  }
  if (a == aend && b != bend)
    return -1;
  if (a != aend && b == bend)
    return 1;
  return 0;
}

static int
vcmpc(int c1, int c2)
{
  if (c1 == c2)
    return 0;
  if (c1 == '~')
    return -1;
  if (c2 == '~')
    return 1;
  if (isdigit_(c1) || !c1)
    return (isdigit_(c2) || !c2) ? 0 : -1;
  if (isdigit_(c2) || !c2)
    return 1;
  if (isalpha_(c1) && c1 != '_')
    return ((isalpha_(c2) && c2 != '_')) ? (int)c1 - c2 : -1;
  if (isalpha_(c2) && c2 != '_')
    return 1;
  return (int)c1 - c2;
}

static size_t
vsfx(const char *s, const char *e)
{
  int expect, sfx;
  size_t clen, len;
  expect = sfx = clen = len = 0;

  for (; s < e; s++) {
    if (expect) {
      expect = 0;
      if (!(isalpha_(*s) && *s != '_') && *s != '~')
        sfx = 0;
    } else if (*s == '.') {
      expect = 1;
      if (!sfx) {
        sfx = 1;
        len = clen;
      }
    } else if (!((isalpha_(*s) && *s != '_') || isdigit_(*s)) && *s != '~') {
     sfx = 0;
    }
    clen++;
  }
  return sfx ? len : clen;
}

static int
vcmpv(const char *a, size_t la, const char *b, size_t lb)
{
  const char *ae = a + la, *be = b + lb;
  int cmp, diff;

  while (a < ae || b < be) {
    diff = 0;
    while ((a < ae && !isdigit_(*a)) || (b < be && !isdigit_(*b))) {
      cmp = vcmpc((a < ae) ? *a : 0, (b < be) ? *b : 0);
      if (cmp)
        return cmp;
      if (a < ae)
        a++;
      if (b < be)
        b++;
    }
    while (a < ae && *a == '0')
      a++;
    while (b < be && *b == '0')
      b++;
    while ((a < ae && isdigit_(*a)) && b < be && isdigit_(*b)) {
      if (!diff)
        diff = (int)*a - (int)*b;
      a++, b++;
    }
    if (a < ae && isdigit_(*a))
      return 1;
    if (b < be && isdigit_(*b))
      return -1;
    if (diff)
      return diff;
  }
  return 0;
}

static int
vercmp(const char *a, size_t la, const char *b, size_t lb)
{
  size_t len1, len2;
  int res, cmp;

  res = bytecmp(a, la, b, lb);
  if (!res)
    return 0;
  if (la < 1)
    return -1;
  if (lb < 1)
    return 1;
  if (la == 1 && *a == '.')
    return -1;
  if (lb == 1 && *b == '.')
    return 1;
  if (la == 2 && a[0] == '.' && a[1] == '.')
    return -1;
  if (lb == 2 && b[0] == '.' && b[1] == '.')
    return 1;
  if (*a == '.' && *b != '.')
    return -1;
  if (*a != '.' && *b == '.')
    return 1;
  if (*a == '.' && *b == '.') {
    a++;
    la--;
    b++;
    lb--;
  }
  len1 = vsfx(a, a + la);
  len2 = vsfx(b, b + lb);
  if (len1 == len2 && !bytecmp(a, len1, b, len2))
    return res;
  cmp = vcmpv(a, len1, b, len2);
  return cmp ? cmp : res;
}

static int
bytecmp(const char *a, size_t la, const char *b, size_t lb)
{
  const char *aend, *bend, *p, *q;
  aend = a + la;
  bend = b + lb;

  for (p = a, q = b; p < aend && q < bend; p++, q++)
    if (*p != *q)
      return (unsigned char)*p - *q;

  if (p == aend && q != bend)
    return -1;
  if (p != aend && q == bend)
    return 1;

  return 0;
}

static int
keycmp(const char *a, size_t la, const char *b, size_t lb, int full)
{
  const char *astrt, *aend;
  const char *bstrt, *bend;
  int ank = 0, bnk = 0;
  int res = 0, flag = 0;

  for (size_t i = 0; i < nkeyd; i++) {
    size_t las, lbs;
    ank = frange(a, la, &keys[i], &astrt, &aend);
    bnk = frange(b, lb, &keys[i], &bstrt, &bend);
    if (ank && bnk)
      continue;
    if (!ank && bnk)
      return 1;
    if (ank && !bnk)
      return -1;
    las = pntlen(astrt, aend);
    lbs = pntlen(bstrt, bend);
    flag = (keys[i].own) ? keys[i].flags : (int)flags;
    if (flag & ver)
      res = vercmp(astrt, las, bstrt, lbs);
    else if (flag & num)
      res = numcmp(astrt, las, bstrt, lbs);
    else if (flag & (dict | icase | inprnt))
      res = dictcmp(astrt, las, bstrt, lbs, flag);
    else
      res = bytecmp(astrt, las, bstrt, lbs);
    if (res)
      return (flag & rev) ? -res : res;
  }
  if (!nkeyd) {
    if (flags & ver)
      res = vercmp(a, la, b, lb);
    else if (flags & num)
      res = numcmp(a, la, b, lb);
    else if (flags & (dict | icase | inprnt))
      res = dictcmp(a, la, b, lb, flags);
    else
      res = bytecmp(a, la, b, lb);
    if (res)
      return (flags & rev) ? -res : res;
  }
  if (!full)
    return 0;
  res = bytecmp(a, la, b, lb);
  return (flags & rev) ? -res : res;
}

#endif /* ENABLE_SORT */
