/* arith.c - shell arithmetic handling functions */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <stddef.h>
#include "arith.h"
#include "var.h"
#include "main.h"
#include "malloc.h"
#include "simd.h"
#include "utils.h"

enum arith_tok {
    A_NUM,
    A_NAME,
    A_EOF,
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
  [A_EOF]    = -1,
};

#define get_lbp(t) ( (t) < A_END ? lbp_tab[t] : 0 )

static const char *ap;    // current position
static size_t alen;       // remaining bytes
static int atok;          // current token type
static long long aval;    // numeric value (for A_NUM)
static const char *aname; // name pointer (for A_NAME)
static size_t anlen;      // name length (for A_NAME)
static char *lname;
static size_t lnlen;

static long long expr_bp(int);
static void next_tok(void);
static long long nud(void);
static long long led(long long);
static long long lookupavar(void);

static inline int
hexval(char c)
{
  if (c >= '0' && c <= '9')
    return c - '0';
  c = c | 1 << 5;
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  return 0;
}

static void
next_tok(void)
{
  size_t skip;
  char c;

  if (alen > 0) {
    skip = simd_skip_spaces(ap, alen);
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
        aval = aval * 16 + hexval(ap[0]);
        ap++;
        alen--;
      }
    } else if (ap[0] == '0') {
      ap++;
      alen--;
      while (alen > 0 && ap[0] >= '0' && ap[0] <= '7') {
        aval = aval * 8 + (ap[0] - '0');
        ap++;
        alen--;
      }
    } else {
      while (alen > 0 && isdigit_(ap[0])) {
        aval = aval * 10 + (ap[0] - '0');
        ap++;
        alen--;
      }
    }
    atok = A_NUM;
    return;
  }

  if (isalpha_(ap[0]) || ap[0] == '_') {
    size_t pos;
    pos = simd_scan_word(ap, alen);
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

static long long
nud(void)
{
  long long val;
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

static long long
led(long long left)
{
  long long rb;
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
      long long rhs;
      char valbuf[32];
      rhs = expr_bp(2);
      if (!lname) {
        shwarn_arg("arithmetic", ap, "left value requried");
        return 0;
      }
      lltoa(rhs, valbuf);
      setvar(lname, valbuf, 0);
      return rhs;
    default:
      atok = A_EOF;
      return left;
  }
}

static long long
expr_bp(int min_bp)
{
  long long left;

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

long long
arith_eval(const char *expr, size_t len)
{
  long long res;

  ap = expr;
  alen = len;
  next_tok();
  res = expr_bp(0);
  return res;
}

static long long
lookupavar(void)
{
  shvar *rvar;
  long long res;

  rvar = findvar_n(aname, anlen);
  if (!rvar)
    return 0;
  if (atoll_(shvar_val(rvar), &res) < 0)
    return 0;
  return res;
}
