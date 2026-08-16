/* arith.c - shell arithmetic handling functions */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stddef.h>
 
#include "arith.h"
#include "errmsg.h"
#include "alloc.h"
#include "expand.h"
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

static const char *ap;     // current position
static size_t alen;        // remaining bytes
static int atok;           // current token type
static i64 aval;        // numeric value (for A_NUM)
static const char *aname;  // name pointer (for A_NAME)
static size_t anlen;       // name length (for A_NAME)
static char *lname;
static size_t lnlen;

static i64 expr_bp(int);
static void next_tok(void);
static i64 nud(void);
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

static void
next_tok(void)
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
      atok = A_DOLLAR;
      break;
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
    case A_DOLLAR:
      lname = NULL;
      {
        i64 dval = 0;
        size_t j = 2, k, elen;

        if (alen > 0 && ap[0] == '(') {
          i64 rv;
          j = 1;
          {
            size_t d = 0;
            for (; j < alen && !(ap[j] == ')' && d == 0); j++) {
              if (ap[j] == '(')
                d++;
              else if (ap[j] == ')')
                d--;
            }
          }
          if (j >= alen) {
            shwarn_arg("arithmetic", ap, "missing ')'");
            atok = A_EOF;
          } else if (ap[1] == '(') {
            if (ap[j - 1] != ')') {
              shwarn_arg("arithmetic", ap, "missing '))'");
              atok = A_EOF;
            } else {
              const char *sap, *saname;
              char *slname;
              size_t salen, sanlen, slnlen;
              int satok;
              i64 saval;

              sap = ap, salen = alen, saval = aval, satok = atok;
              saname = aname, sanlen = anlen;
              slname = lname, slnlen = lnlen;
              rv = arith_eval(ap + 2, j - 2);
              ap = sap, alen = salen, atok = satok, aval = saval;
              aname = saname, anlen = sanlen;
              lname = slname, lnlen = slnlen;

              ap += j + 2, alen -= j + 2;
              dval = rv;
            }

          } else {
            char *cmdsub;
            size_t sublen = 0;
            if ((cmdsub = exp_cmdsub(ap + 1, j - 1, &sublen))) {
              char *sbuf = st_strndup(cmdsub, sublen);
              if (atoll_(sbuf, &dval) < 0)
                dval = 0;
            }
            ap += j + 1;
            alen -= j + 1;
          }
        } else if (alen > 0 && ap[0] == '{') {
          j = 1;
          {
            size_t d = 0;
            for (; j < alen && !(ap[j] == '}' && d == 0); j++) {
              if (ap[j] == '{')
                d++;
              else if (ap[j] == '}')
                d--;
            }
          }
          if (j >= alen) {
            shwarn_arg("arithmetic", ap, "missing '}'");
            atok = A_EOF;
          } else {
            size_t clen = j - 1;
            int plain = clen > 0;
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
              if (n > 0 && n <= (size_t)SHARGC && SHARGV[n - 1])
                atoll_(SHARGV[n - 1], &dval);
            } else if (plain) {
              dval = avarval(ap + 1, clen);
            } else {
              wf f;
              f.qs = QBRACE_DQ;
              f.word = (char *)ap;
              f.len = j + 1;
              f.next = NULL;
              f.flags = 0;
              {
                wf *ew = exp_word(&f, &elen);
                char *estr = ew ? join_wf(ew, 0) : (char *)"";
                if (atoll_(estr, &dval) < 0)
                  dval = 0;
              }
            }
            ap += j + 1;
            alen -= j + 1;
          }
        } else if (alen > 0 && (isalpha_(ap[0]) || ap[0] == '_')) {
          size_t nlen = sscnword(ap, alen);
          dval = avarval(ap, alen);
          ap += nlen;
          alen -= nlen;
        } else if (alen > 0) {
          switch (ap[0]) {
            case '?':
              dval = LSTATUS;
              ap++, alen--;
              break;
            case '$':
              if (atoll_(gvar.pid_s, &dval) < 0)
                dval = 0;
              ap++, alen--;
              break;
            case '!':
              if (atoll_(gvar.bgpid_s, &dval) < 0)
                dval = 0;
              ap++, alen--;
              break;
            case '#':
              dval = SHARGC;
              ap++, alen--;
              break;
            case '-':
              ap++, alen--;
              break;
            default:
              if (isdigit_(ap[0])) {
                size_t n = 0, k = 0;
                while (k < alen && isdigit_(ap[k]))
                  n = n * 10 + (ap[k++] - 0);
                if (n > 0 && n <= (size_t)SHARGC && SHARGV[n - 1])
                  atoll_(SHARGV[n - 1], &dval);
                ap += k, alen -= k;
              } else {
                shwarn_arg("arithmetic", ap, "unexpected $");
              }
              break;
          }
        }
        next_tok();
        return dval;
      }
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
