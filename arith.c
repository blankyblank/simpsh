/* arith.c - shell arithmetic handling functions */
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#include <ctype.h>
#include <stddef.h>
 
#include "arith.h"
#include "errmsg.h"
#include "alloc.h"
#include "expand.h"
#include "lex.h"
#include "input.h"
#include "var.h"
#include "main.h"
#include "simd.h"
#include "utils.h"

enum arith_tok {
    A_NUM,
    A_NAME,
    A_EOF,
    A_DOLLAR,   // $
    A_PLUS,     // +
    A_MINUS,    // -
    A_STAR,     // *
    A_SLASH,    // /
    A_PCT,      // %
    A_LPAREN,   // (
    A_RPAREN,   // )
    A_LSHIFT,   // <<
    A_RSHIFT,   // >>
    A_LT,       // <
    A_GT,       // >
    A_LE,       // <=
    A_GE,       // >=
    A_ASSN,     // =
    A_EQ,       // ==
    A_NE,       // !=
    A_BAND,     // &
    A_BXOR,     // ^
    A_BOR,      // |
    A_LAND,     // &&
    A_LOR,      // ||
    A_BNOT,     // ~
    A_LNOT,     // !
    A_END,      // end marker
};

static const int lbp_tab[] = {
  [A_LOR]    = 4,
  [A_LAND]   = 5,
  [A_BOR]    = 6,
  [A_BXOR]   = 7,
  [A_BAND]   = 8,
  [A_EQ]     = 9,
  [A_NE]     = 9,
  [A_LT]     = 10,
  [A_GT]     = 10,
  [A_LE]     = 10,
  [A_GE]     = 10,
  [A_LSHIFT] = 11,
  [A_RSHIFT] = 11,
  [A_PLUS]   = 12,
  [A_MINUS]  = 12,
  [A_STAR]   = 13,
  [A_SLASH]  = 13,
  [A_PCT]    = 13,
  [A_ASSN]   = 2,
  [A_BNOT]   = -1,
  [A_LNOT]   = -1,
  [A_LPAREN] = -1,
  [A_RPAREN] = -1,
  [A_EOF]    = -1,
};

/* NOLINTBEGIN(readability-magic-numbers) */
#define get_lbp(t) ((t) < A_END ? lbp_tab[t] : 0)

static const char *ap;     /* current position */
static size_t alen;        /* remaining bytes */
static int atok;           /* current token type */
static i64 aval;           /* numeric value (for A_NUM) */
static const char *aname;  /* name pointer (for A_NAME) */
static size_t anlen;       /* name length (for A_NAME) */
static char *lname;
static size_t lnlen;
static const char *rpy;    /* pending sliced text */
static size_t rlen;        /* bytes left in rpy */

static i64 expr_bp(int);
static void next_tok(void);
static void scan_tok(void);
static i64 nud(void);
static int arith_dollar(i64 *num, const char **txt, size_t *tlen);
static i64 led(i64);
static i64 lookupavar(void);

static inline i64
avarval(const char *name, size_t nlen)
{
  shvar *v;
  i64 res;

  if (!(v = findvar_n(name, nlen)))
    return 0;
  if (v->flags & VINT)
    return v->ival;
  if (atoll_(shvar_val(v), &res) < 0)
    return 0;
  return res;
}

static inline const char *
avarstr(const char *name, size_t nlen, size_t *vlen)
{
  shvar *v;

  if (!(v = findvar_n(name, nlen)))
    return NULL;
  *vlen = vallen(v);
  return shvar_val(v);
}

static void
next_tok(void)
{
  for (;;) {
    if (rlen > 0) {
      const char *sap;
      size_t salen, skip;

      sap = ap;
      salen = alen;
      skip = sskipspace(rpy, rlen);
      if (rlen - skip == 0) {
        rpy += skip;
        rlen = 0;
        ap = sap;
        alen = salen;
        continue;
      }
      ap = rpy;
      alen = rlen;
      scan_tok();
      rpy = ap;
      rlen = alen;
      ap = sap;
      alen = salen;
      return;
    }

    {
      size_t skip;
      skip = sskipspace(ap, alen);
      ap += skip;
      alen -= skip;
    }
    if (!alen) {
      atok = A_EOF;
      return;
    }

    if (*ap == '$') {
      i64 num;
      const char *txt;
      size_t tlen;

      txt = NULL;
      tlen = 0;

      ap++;
      alen--;
      switch (arith_dollar(&num, &txt, &tlen)) {
        case 1:
          atok = A_NUM;
          aval = num;
          return;
        case 0:
          if (!tlen)
            continue;
          rpy = txt;
          rlen = tlen;
          continue;
        default:
          atok = A_EOF;
          return;
      }
    }
    scan_tok();
    return;
  }
}

static void
scan_tok(void)
{
  size_t skip;
  char c;

  /* TODO: decide if i want to keep simd function
   * or move to scalar */
  if (alen > 0) {
    skip = sskipspace(ap, alen);
    ap += skip;
    alen -= skip;
  }
  if (!alen) {
    atok = A_EOF;
    return;
  }

  if (isdigit_(ap[0])) {
    aval = 0;
    if (ap[0] == '0' && alen > 1 && ap[1] == 'x') {
      ap += 2;
      alen -= 2;
      while (alen > 0 && isxdigit(ap[0])) {
        aval = (aval * 16) + hexval(ap[0]);
        ap++;
        alen--;
      }
    } else if (ap[0] == '0') {
      ap++;
      alen--;
      while (alen > 0 && ap[0] >= '0' && ap[0] <= '7') {
        aval = (aval * 8) + (ap[0] - '0');
        ap++;
        alen--;
      }
    } else {
      while (alen > 0 && isdigit_(ap[0])) {
        aval = (aval * 10) + (ap[0] - '0');
        ap++;
        alen--;
      }
    }
    atok = A_NUM;
    return;
  }

  if (isalpha_(ap[0]) || ap[0] == '_') {
    size_t pos;
    pos = sscnword(ap, alen);
    aname = ap;
    anlen = pos;
    ap += pos;
    alen -= pos;
    atok = A_NAME;
    return;
  }

  c = ap[0];
  ap++;
  alen--;
  switch (c) {
    case '+':
      atok = A_PLUS;
      break;
    case '-':
      atok = A_MINUS;
      break;
    case '*':
      atok = A_STAR;
      break;
    case '/':
      atok = A_SLASH;
      break;
    case '%':
      atok = A_PCT;
      break;
    case '~':
      atok = A_BNOT;
      break;
    case '^':
      atok = A_BXOR;
      break;
    case '(':
      atok = A_LPAREN;
      break;
    case ')':
      atok = A_RPAREN;
      break;
    case '$':
      shwarn_arg("arithmetic", ap, "unexpected $");
      atok = A_EOF;
      return;
    case '<':
      if (alen > 0 && ap[0] == '<') {
        atok = A_LSHIFT;
        ap++;
        alen--;
      } else if (alen > 0 && ap[0] == '=') {
        atok = A_LE;
        ap++;
        alen--;
      } else {
        atok = A_LT;
      }
      break;
    case '>':
      if (alen > 0 && ap[0] == '>') {
        atok = A_RSHIFT;
        ap++;
        alen--;
      } else if (alen > 0 && ap[0] == '=') {
        atok = A_GE;
        ap++;
        alen--;
      } else {
        atok = A_GT;
      }
      break;
    case '=':
      if (alen > 0 && ap[0] == '=') {
        atok = A_EQ;
        ap++;
        alen--;
      } else {
        atok = A_ASSN;
        return;
      }
      break;
    case '!':
      if (alen > 0 && ap[0] == '=') {
        atok = A_NE;
        ap++;
        alen--;
      } else {
        atok = A_LNOT;
      }
      break;
    case '&':
      if (alen > 0 && ap[0] == '&') {
        atok = A_LAND;
        ap++;
        alen--;
      } else {
        atok = A_BAND;
      }
      break;
    case '|':
      if (alen > 0 && ap[0] == '|') {
        atok = A_LOR;
        ap++;
        alen--;
      } else {
        atok = A_BOR;
      }
      break;
    default:
      shwarn_arg("arithmetic syntax", ap, "unexpected operator");
      atok = A_EOF;
      return;
  }
}

static i64
nud(void)
{
  i64 val;
  switch (atok) {
    case A_NUM:
      lname = NULL;
      val = aval;
      next_tok();
      return val;
    case A_NAME:
      lname = st_strndup(aname, anlen);
      lnlen = anlen;
      val = lookupavar();
      next_tok();
      return val;
    case A_LPAREN:
      lname = NULL;
      next_tok();
      val = expr_bp(0);
      if (atok != A_RPAREN) {
        shwarn_arg("arithmetic syntax", ap, "expected ')'");
        return 0;
      }
      next_tok();
      return val;
    case A_MINUS:
      lname = NULL;
      next_tok();
      return -expr_bp(14);
    case A_LNOT:
      lname = NULL;
      next_tok();
      return !expr_bp(14);
    case A_BNOT:
      lname = NULL;
      next_tok();
      return ~expr_bp(14);
    case A_PLUS:
      lname = NULL;
      next_tok();
      return expr_bp(14);
    default:
      shwarn_arg("arithmetic syntax", ap, "unexpected token");
      atok = A_EOF;
      return 0;
  }
}

static int
arith_dollar(i64 *num, const char **txt, size_t *tlen)
{
  if (!alen) {
    shwarn_arg("arithmetic", ap, "unexpected $");
    return -1;
  }
  if (ap[0] == '(') {
    size_t j = 2, d = 0;

    for (; j < alen && !(ap[j] == ')' && d == 0); j++) {
      if (ap[j] == '(')
        d++;
      else if (ap[j] == ')')
        d--;
    }
    if (j >= alen) {
      shwarn_arg("arithmetic", ap, "missing ')'");
      return -1;
    } else if (ap[1] == '(') {
      const char *sap, *saname;
      char *slname;
      size_t salen, sanlen, slnlen;
      int satok;
      i64 saval, rv;

      sap = ap, salen = alen, saval = aval, satok = atok;
      saname = aname, sanlen = anlen;
      slname = lname, slnlen = lnlen;
      rv = arith_eval(ap + 2, j - 2);
      ap = sap, alen = salen, atok = satok, aval = saval;
      aname = saname, anlen = sanlen;
      lname = slname, lnlen = slnlen;
      ap += j + 2, alen -= j + 2;
      *num = rv;
      return 1;
    } else {
      char *cmdsub;
      if ((cmdsub = exp_cmdsub(ap + 1, j - 1, tlen))) {
        *txt = cmdsub;
        ap += j + 1;
        alen -= j + 1;
        return 0;
      }
      return -1;
    }
  } else if (ap[0] == '{') {
    size_t j = 1, d = 0, k, clen;
    int plain;

    for (; j < alen && !(ap[j] == '}' && d == 0); j++) {
      if (ap[j] == '{')
        d++;
      else if (ap[j] == '}')
        d--;
    }
    if (j >= alen) {
      shwarn_arg("arithmetic", ap, "missing '}'");
      return -1;
    }
    clen = j - 1;
    plain = clen > 0;
    for (k = 1; k < j; k++) {
      char cc = ap[k];
      if (!(isalpha_(cc) || isdigit_(cc) || cc == '_')) {
        plain = 0;
        break;
      }
    }
    if (plain && isdigit_(ap[1])) {
      size_t n = 0, t = 0;
      while (t < clen && isdigit_(ap[1 + t]))
        n = n * 10 + (ap[1 + t++] - '0');
      if (n > 0 && n <= (size_t)SHARGC && SHARGV[n - 1]) {
        *txt = SHARGV[n - 1];
        *tlen = strlen(SHARGV[n - 1]);
      } else {
        *tlen = 0;
      }
    } else if (plain) {
      if (!(*txt = avarstr(ap + 1, clen, tlen)))
        *tlen = 0;
    } else {
      wf *ew, *ewx;
      char *wbuf, *estr;
      int ch;

      wbuf = st_alloc(j + 3);
      wbuf[0] = '$';
      memcpy(wbuf + 1, ap, j + 1);
      wbuf[j + 2] = '\0';
      setinputstrn(wbuf, j + 2);
      ch = shgetchar();
      ewx = get_wf(ch);
      popinput();
      ew = ewx ? exp_word(ewx, &clen) : NULL;
      if ((estr = ew ? join_wf(ew, 0) : NULL)) {
        *txt = estr;
        *tlen = strlen(estr);
      } else {
        *tlen = 0;
      }
    }
    ap += j + 1;
    alen -= j + 1;
    return 0;
  } else if (isalpha_(ap[0]) || ap[0] == '_') {
    size_t nlen = sscnword(ap, alen);

    if ((*txt = avarstr(ap, nlen, tlen)) == NULL)
      *tlen = 0; /* unset -> empty splice */
    ap += nlen;
    alen -= nlen;
    return 0;
  } else if (isdigit_(ap[0])) {
    size_t n = 0, k = 0;
    while (k < alen && isdigit_(ap[k]))
      n = n * 10 + (ap[k++] - '0'); /* fixed: - '0' */
    if (n > 0 && n <= (size_t)SHARGC && SHARGV[n - 1]) {
      *txt = SHARGV[n - 1];
      *tlen = strlen(SHARGV[n - 1]);
    } else {
      *tlen = 0;
    }
    ap += k;
    alen -= k;
    return 0;
  } else {
    switch (ap[0]) {
      case '?':
        ap++, alen--;
        *num = LSTATUS;
        return 1;
      case '$':
        ap++, alen--;
        if (atoll_(gvar.pid_s, num) < 0)
          *num = 0;
        return 1;
      case '!':
        ap++, alen--;
        if (atoll_(gvar.bgpid_s, num) < 0)
          *num = 0;
        return 1;
      case '#':
        ap++, alen--;
        *num = SHARGC;
        return 1;
      case '-':
        {
          static const char dashopts[] = "abCefhiImnsuvVx";
          static char dashbuf[16];
          size_t k;

          ap++, alen--;
          for (k = 0; k < 15; k++)
            dashbuf[k] = GETSHOPT(k) ? dashopts[k] : 0;
          *txt = dashbuf;
          *tlen = strlen(dashbuf);
          return 0;
        }
      default:
        shwarn_arg("arithmetic", ap, "unexpected $");
        return -1;
    }
  }
}

static i64
led(i64 left)
{
  i64 rb;
  switch (atok) {
    case A_PLUS:
      next_tok();
      return left + expr_bp(13);
    case A_MINUS:
      next_tok();
      return left - expr_bp(13);
    case A_STAR:
      next_tok();
      return left * expr_bp(14);
    case A_SLASH:
      next_tok();
      rb = expr_bp(14);
      if (!rb) {
        shwarn_arg("arithmetic", ap, "division by 0");
        return 0;
      }
      return left / rb;
    case A_PCT:
      next_tok();
      rb = expr_bp(14);
      if (!rb) {
        shwarn_arg("arithmetic", ap, "division by 0");
        return 0;
      }
      return left % rb;
    case A_LSHIFT:
      next_tok();
      return left << expr_bp(12);
    case A_RSHIFT:
      next_tok();
      return left >> expr_bp(12);
    case A_LT:
      next_tok();
      return left < expr_bp(11);
    case A_GT:
      next_tok();
      return left > expr_bp(11);
    case A_LE:
      next_tok();
      return left <= expr_bp(11);
    case A_GE:
      next_tok();
      return left >= expr_bp(11);
    case A_EQ:
      next_tok();
      return left == expr_bp(10);
    case A_NE:
      next_tok();
      return left != expr_bp(10);
    case A_BAND:
      next_tok();
      return left & expr_bp(9);
    case A_BXOR:
      next_tok();
      return left ^ expr_bp(8);
    case A_BOR:
      next_tok();
      return left | expr_bp(7);
    case A_LAND:
      next_tok();
      return left && expr_bp(6);
    case A_LOR:
      next_tok();
      return left || expr_bp(5);
    case A_ASSN:
      next_tok();
      i64 rhs;
      char valbuf[32];
      char *name = lname;
      rhs = expr_bp(2);
      if (!name) {
        shwarn_arg("arithmetic", ap, "left value requried");
        return 0;
      }
      lltoa(rhs, valbuf);
      setvar_i(name, valbuf, rhs, 0);
      return rhs;
    default:
      atok = A_EOF;
      return left;
  }
}

static i64
expr_bp(int min_bp)
{
  i64 left;

  left = nud();
  for (;;) {
    int bp;
    bp = get_lbp(atok);
    if (bp < min_bp)
      break;
    left = led(left);
  }
  return left;
}

i64
arith_eval(const char *expr, size_t len)
{
  i64 res;
  const char *p = expr;
  size_t n = len, skip;

  skip = sskipspace(p, n);
  p += skip, n -= skip;
  if (n > 0) {
    i64 lval, rval;
    size_t wlen;
    int op = 0;

    /* left operand */
    if (isdigit_(p[0])) {
      lval = 0;
      while (n > 0 && isdigit_(p[0])) {
        lval = lval * 10 + (p[0] - '0');
        p++, n--;
      }
    } else if (isalpha_(p[0]) || p[0] == '_') {
      wlen = sscnword(p, n);
      lval = avarval(p, wlen);
      p += wlen, n -= wlen;
      if (lval < 0)
        goto fallback;
    } else if (p[0] == '$' && n > 1 && (isalpha_(p[1]) || p[1] == '_')) {
      p++, n--;
      wlen = sscnword(p, n);
      lval = avarval(p, wlen);
      p += wlen, n -= wlen;
    } else {
      goto fallback;
    }

    skip = sskipspace(p, n);
    p += skip, n -= skip;

    /* operator */
    if (n == 0)
      goto fallback;
    if (p[0] == '+') {
      op = '+';
      p++, n--;
    } else if (p[0] == '-') {
      op = '-';
      p++, n--;
    } else if (p[0] == '*') {
      op = '*';
      p++, n--;
    } else if (p[0] == '/') {
      op = '/';
      p++, n--;
    } else if (p[0] == '%') {
      op = '%';
      p++, n--;
    } else if (p[0] == '<' && n > 1 && p[1] == '<') {
      op = '<';
      p += 2, n -= 2;
    } else if (p[0] == '>' && n > 1 && p[1] == '>') {
      op = '>';
      p += 2, n -= 2;
    } else if (p[0] == '&') {
      op = '&';
      p++, n--;
    } else if (p[0] == '^') {
      op = '^';
      p++, n--;
    } else if (p[0] == '|') {
      op = '|';
      p++, n--;
    } else
      goto fallback;

    skip = sskipspace(p, n);
    p += skip, n -= skip;

    /* right operand */
    if (isdigit_(p[0])) {
      rval = 0;
      while (n > 0 && isdigit_(p[0])) {
        rval = rval * 10 + (p[0] - '0');
        p++, n--;
      }
    } else if (isalpha_(p[0]) || p[0] == '_') {
      wlen = sscnword(p, n);
      rval = avarval(p, wlen);
      p += wlen, n -= wlen;
      if (rval < 0)
        goto fallback;
    } else if (p[0] == '$' && n > 1 && (isalpha_(p[1]) || p[1] == '_')) {
      p++, n--;
      wlen = sscnword(p, n);
      rval = avarval(p, wlen);
      p += wlen, n -= wlen;
      if (rval < 0)
        goto fallback;
    } else {
      goto fallback;
    }

    skip = sskipspace(p, n);
    n -= skip;
    if (n == 0) {
      switch (op) {
        case '+':
          return lval + rval;
        case '-':
          return lval - rval;
        case '*':
          return lval * rval;
        case '/':
          if (rval == 0)
            goto fallback;
          return lval / rval;
        case '%':
          if (rval == 0)
            goto fallback;
          return lval % rval;
        case '<':
          return lval << (rval & 63);
        case '>':
          return lval >> (rval & 63);
        case '&':
          return lval & rval;
        case '^':
          return lval ^ rval;
        case '|':
          return lval | rval;
      }
    }
  }

fallback:
  ap = expr;
  alen = len;
  next_tok();
  res = expr_bp(0);
  return res;
}

/* why are we keeping this?????????????????????? */
static i64
lookupavar(void)
{
  shvar *rvar;
  i64 res;

  rvar = findvar_n(aname, anlen);
  if (!rvar)
    return 0;
  if (rvar->flags & VINT)
    return rvar->ival;
  if (atoll_(shvar_val(rvar), &res) < 0)
    return 0;
  return res;
}


/* NOLINTEND(readability-magic-numbers) */
