/* lex.c - tokenizer functions */

/* NOLINTBEGIN(readability-function-cognitive-complexity) */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "arith.h"
#include "env.h"
#include "errmsg.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "simd.h"
#include "utils.h"

wf *wf_chunk = NULL;
static wf *head = NULL;
static wf *tail = NULL;
static size_t wflen;
unsigned int wf_chunk_left = 0;
int alias_depth = 0;
int notclosed = 0;
int chkwd = 0;

#define CTX_MAX     8
/* current context */
#define cctx      (ctx_stack[ctx_depth])
#define pshctx(m) (ctx_stack[++ctx_depth] = (m))
#define popctx()  (ctx_depth--)
#define NCHR(c)   (nchars[(unsigned char)(c)])
#define DCHR(c)   (dqchars[(unsigned char)(c)])
#define SCHR(c)   (sqchars[(unsigned char)(c)])
#define flushword(qs) \
  do { \
    if (wflen > 0) { \
      append_wf(&head, &tail, grab_str(wflen), wflen, qs); \
      wflen = 0; \
    } \
  } while (0)

typedef enum {
  M_NORMAL,
  M_DQUOTE,
  M_SQUOTE
} tokmode;

enum qs {
  insq = (1 << 0),
  indq = (1 << 1),
  esc = (1 << 2),
};

/* clang-format off */
static const unsigned char nchars[256] = {
  [' '] = C_SPACE,
  ['\t'] = C_SPACE,
  ['\n'] = C_NL,
  ['#'] = C_COMMENT,
  ['\''] = C_SQUOTE,
  ['"'] = C_DQUOTE,
  ['\\'] = C_BSLASH,
  ['$'] = C_DOLLAR,
  ['&'] = C_AMP,
  ['|'] = C_PIPE,
  [';'] = C_SEMI,
  ['('] = C_LP,
  [')'] = C_RP,
  ['{'] = C_LB,
  ['}'] = C_RB,
  ['<'] = C_LT,
  ['>'] = C_GT,
  ['`'] = C_BTICK,
}; /* everything else is 0 = C_WORD */

static const unsigned char dqchars[256] = {
  ['"'] = C_DQUOTE,
  ['\\'] = C_BSLASH,
  ['$'] = C_DOLLAR,
  ['`'] = C_BTICK,
};

static const unsigned char sqchars[256] = {
  ['\''] = C_SQUOTE,
};

static const unsigned char *ctx_tables[] = {
  [M_NORMAL] = nchars,
  [M_DQUOTE] = dqchars,
  [M_SQUOTE] = sqchars,
};

const struct kw kw[32] = {
  [25] = { "!",      1, TNOT },
  [14] = { "do",     2, TDO  },
  [1]  = { "if",     2, TIF  },
  [17] = { "in",     2, TIN  },
  [4]  = { "fi",    2, TFI   },
  [12] = { "for",   3, TFOR  },
  [5]  = { "case",  4, TCASE },
  [7]  = { "else",  4, TELSE },
  [9]  = { "elif",  4, TELIF },
  [3]  = { "esac",  4, TESAC },
  [8]  = { "then",  4, TTHEN },
  [6]  = { "done",  4, TDONE },
  [15] = { "while", 5,TWHILE },
  [27] = { "until", 5,TUNTIL },
}; /* clang-format on */

static int ctx_depth;
static tokmode ctx_stack[CTX_MAX] = { M_NORMAL };

static wf *get_wf(int);
static sh_tok tokword(wf *, int*);
static void tokws(void);
static sh_tok toklt(void);
static sh_tok toknl(void);
static sh_tok tokgt(void);
static void lexsquote(void);
static void lexdquote(void);
static int lexbslash(int);
static int lexcmdsub(void);
static int lexarith(void);
static int lexvbrace(void);
static int lexvar(int);
static int lexvspecial(int);
static int lexvnum(int);
static int lexbtick(void);
static void skipcomment(void);

#define qescape(c) \
  case insq: \
    if ((c) == '\'') { \
      cstate &= ~(unsigned int)insq; \
      st_putc(c); \
      cmdlen++; \
      continue; \
    } else { \
      st_putc(c); \
      cmdlen++; \
      continue; \
    } \
  case esc: \
    st_putc(c); \
    cmdlen++; \
    cstate &= ~(unsigned int)esc; \
    continue; \
  case indq: \
    if ((c) == '"') { \
      cstate &= ~(unsigned int)indq; \
      st_putc(c); \
      cmdlen++; \
      continue; \
    } else if ((c) == '\\') { \
      st_putc(c); \
      cmdlen++; \
      cstate |= esc; \
      continue; \
    } else { \
      st_putc(c); \
      cmdlen++; \
      continue; \
    }

static inline int
eatbnl(void)
{
  char c, n;

  while ((c = (char)shgetchar()) == '\\') {
    if ((n = shgetchar()) == '\n') {
      shinpt->linenum++;
      continue;
    }
    if (n != SHEOF) {
      shungetc(n);
      return '\\';
    }
  }
  return c;
}

/* combines word fragments into a string */
char *
join_wf(wf *wordf)
{
  wf *f = wordf;
  char *s, *buf;
  size_t len = 0;

  if (f && !f->next && f->word)
    return f->word;

  for (f = wordf; f && f->word; f = f->next)
    len += f->len;
  buf = st_alloc(len + 1);
  s = buf;
  for (f = wordf; f && f->word; f = f->next) {
    s = mempcpy_(s, f->word, f->len);
    *s = '\0';
  }
  return buf;
}

/** Get wf's from input */
__attribute__((hot)) static wf *
get_wf(int c)
{
  char *w;
  wf_chunk = NULL;
  wf_chunk_left = 0;
  wflen = 0;
  head = NULL;
  tail = NULL;

  for (;;) {
    if (cctx == M_NORMAL) {
      if (stleft < 32)
        grow_stack(32);
      while (nchars[(unsigned char)c] == C_WORD) {
        *(unsigned char *)stnext++ = c, stleft--;
        wflen++;
        c = eatbnl();
        if (c == SHEOF)
          goto done;
      }
    }
    int state = ctx_tables[(ctx_stack[ctx_depth])][(unsigned char)c];
    switch (state) {
      case C_SQUOTE:
        lexsquote();
        break;
      case C_DQUOTE:
        lexdquote();
        break;
      case C_BSLASH:
        if (lexbslash(c) == SHEOF)
          goto done;
        break;
      case C_DOLLAR:
        {
          int n, n2;
          if ((n = eatbnl()) == SHEOF) {
            notclosed = 1;
            goto done;
          }
          if (n == '(') {
            n2 = eatbnl();
            if (n2 == '(') {
              if (lexarith() == SHEOF)
                goto done;
            } else {
              shungetc(n2);
              if (lexcmdsub() == SHEOF)
                goto done;
            }
          } else if (n == '{') {
            if (lexvbrace() == SHEOF)
              goto done;
          } else if (isalpha_(n) || n == '_') {
            if (lexvar(n) == SHEOF)
              goto done;
          } else if (n == '$' || n == '?' || n == '!' || /* $$ $? $! $# */
                     n == '#' || n == '@' || n == '*' || n == '-') {
            if (lexvspecial(n) == SHEOF)
              goto done;
          } else if (isdigit_(n)) {
            if (lexvnum(n) == SHEOF)
              goto done;
          } else {
            stcheck(32);
            st_putc(c);
            wflen++;
            shungetc(n);
          }
          break;
        }
      case C_BTICK:
        c = lexbtick();
        if (c == SHEOF)
          goto done;
        break;

      case C_AMP:
      case C_PIPE:
      case C_SEMI:
      case C_LP:
      case C_RP:
      case C_LB:
      case C_RB:
      case C_LT:
      case C_GT:
      case C_SPACE:
      case C_NL:
        if (cctx == M_NORMAL) {
          shungetc(c);
          goto done;
        }
      /* falls through */
      default:
      case C_WORD:
      case C_COMMENT:
        if (c == '\n')
          shinpt->linenum++;
        stcheck(32), st_putc(c);
        wflen++;
        break;
    }
    c = (cctx == M_SQUOTE) ? shgetchar() : eatbnl();
    if (c == SHEOF) {
      if (cctx != M_NORMAL)
        notclosed = 1;
      goto done;
    }
  }

done:
  /*  one last save  */
  if (wflen) { /* clang-format off */
    w = grab_str(wflen);
    append_wf(&head, &tail, w, wflen, (cctx == M_SQUOTE) ? QSINGLE : (cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  } else if (!notclosed) {
    w = grab_str(0);
    append_wf(&head, &tail, w, 0, (cctx == M_SQUOTE) ? QSINGLE : (cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  }
  return head; /* clang-format on */
}

/** create sh_toks out of line */
__attribute__((hot)) sh_tok
tokenize(void)
{
  int wd, c, n;
  wf *f;

  wd = chkwd;
  chkwd = 0;

  while ((c = shgetchar()) != SHEOF) {
    switch (NCHR(c)) {
      case C_SPACE:
        tokws();
        continue;
      case C_COMMENT:
        skipcomment();
        continue;
      case C_NL:
        if (wd & CHKNL)
          continue;
        return toknl();
      case C_AMP:
        n = eatbnl();
        if (n == '&')
          return SHTOK(TAND);
        if (n != SHEOF)
          shungetc(n);
        return SHTOK(TBKGRND);
      case C_PIPE:
        n = eatbnl();
        if (n == '|')
          return SHTOK(TOR);
        if (n != SHEOF)
          shungetc(n);
        return SHTOK(TPIPE);
      case C_SEMI:
        n = eatbnl();
        if (n == ';')
          return SHTOK(TDSEMI);
        if (n != SHEOF)
          shungetc(n);
        return SHTOK(TSEMI);
      case C_LP:
        return SHTOK(TLP);
      case C_RP:
        return SHTOK(TRP);
      case C_LB:
        return SHTOK(TLB);
      case C_RB:
        return SHTOK(TRB);
      case C_LT:
        return toklt();
      case C_GT:
        return tokgt();
      case C_BSLASH:
        if ((n = shgetchar()) == '\n') {
          shinpt->linenum++;
          continue;
        }
        if (n != SHEOF)
          shungetc(n);
      /* falls through */
      default:
        f = get_wf(c);
        if (!f)
          return SHTOK(TEOF);
        sh_tok t = tokword(f, &wd);
        if (t.type == TCONT)
          continue;
        return t;

        /* AHEAD OF TIME SCAN */
    }
  }
  return SHTOK(TEOF);
}

static sh_tok
tokword(wf *f, int *wd)
{
  char *word;
  alias *a;
  f->flags = 0;
  if (f->qs == QNONE && !f->next)
    f->flags |= WFSINGLE;
  for (wf *p = f; p; p = p->next) {
    if (p->qs != QNONE)
      f->flags |= WFDOUBLE;
    if (p->qs == QCMDSUB || p->qs == QCMDSUB_DQ)
      f->flags |= WFCMDSUB;
  }
  if (*wd & CHKKWD && (f->flags & WFSINGLE)) {
    int h = kwhash(f->word, f->len);
    if (kw[h].word && kw[h].len == f->len &&
        memcmp(kw[h].word, f->word, f->len) == 0)
      return (sh_tok) { .type = kw[h].tok, .cmd = (f) };
  }
  if ((*wd & CHKALIAS) && (f->flags & WFSINGLE)) {
    word = join_wf(f);
    a = findalias(word);
    if (a) {
      if (alias_depth >= MAX_ALIAS_DEPTH) {
        fprintf(stderr, "alias: too many levels of recursion\n");
        return SHTOK(TEOF);
      }
      pushstring(a->value, strlen(a->value), 1);
      *wd &= ~CHKALIAS;
      return SHTOK(TCONT);
    }
  }
  return SHWORD(f);
}

static void
tokws(void)
{
  const char *buf;
  size_t avail = shpeek(&buf);
  if (avail >= 16) {
    size_t skip = sskipspace(buf, avail);
    if (skip > 0)
      shadvance(skip);
  }
}

static sh_tok
toknl(void)
{
  int c;
  const char *buf;
  size_t avail;
  while ((avail = shpeek(&buf)) > 0) {
    size_t skip;
    skip = sskipnl(buf, avail);
    if (skip > 0)
      shadvance(skip);
    if (skip < avail)
      break;
  }
  c = shgetchar();
  if (c != '\n' && c != SHEOF)
    shungetc(c);
  return SHTOK(TNL);
}

static sh_tok
toklt(void)
{
  int n;
  n = eatbnl();
  if (n == '<') {
    if ((n = eatbnl()) == '-')
      return SHREDIR(RDHERE_D);
    shungetc(n);
    return SHREDIR(RDHERE);
  }
  if (n == '&')
    return SHREDIR(RDDUPI);
  if (n == '>')
    return SHREDIR(RDRW);
  shungetc(n);
  return SHREDIR(RDIN);
}

static sh_tok
tokgt(void)
{
  int n;
  n = eatbnl();
  if (n == '>')
    return SHREDIR(RDAPP);
  if (n == '&')
    return SHREDIR(RDDUPO);
  if (n == '|')
    return SHREDIR(RDCLOB);
  shungetc(n);
  return SHREDIR(RDOUT);
}

static int
lexbslash(int c)
{
  int n;
  char *w;
  if ((n = shgetchar()) == '\n') {
    shinpt->linenum++;
    return 0;
  }
  if (n == SHEOF)
    return SHEOF;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  if (cctx == M_DQUOTE && n != '$' && n != '"' && n != '\\' && n != '`') {
    stcheck(32), st_putc(c);
    wflen++;
  }
  stcheck(32), st_putc(n);
  wflen++;
  w = grab_str(wflen);
  append_wf(&head, &tail, w, wflen, QSINGLE);
  wflen = 0;
  return 0;
}

static int
lexcmdsub(void)
{
  /* $() */
  int cmdsubd = 1, c;
  size_t cmdlen;
  enum qs cstate;
  char *w;
  cstate = 0;
  cmdlen = 0;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);

  stcheck(32);
  for (int ch = shgetchar();; ch = shgetchar()) {
    if (ch == SHEOF) {
      notclosed = 1;
      return SHEOF;
    }
    stcheck(32);
    switch (cstate) {
      qescape(ch)
    }
    switch (ch) {
      case '\'':
        cstate |= insq;
        stcheck(32);
        st_putc(ch);
        cmdlen++;
        break;
      case '"':
        stcheck(32);
        st_putc(ch);
        cmdlen++;
        cstate |= indq;
        break;
      case '\\':
        cstate |= esc;
        stcheck(32);
        st_putc(ch);
        cmdlen++;
        break;
      case '(':
        stcheck(32);
        st_putc(ch);
        cmdlen++;
        cmdsubd++;
        break;
      case ')':
        cmdsubd--;
        if (!cmdsubd) {
          goto end;
        }
        stcheck(32);
        st_putc(ch);
        cmdlen++;
        break;
      default:
        stcheck(32);
        st_putc(ch);
        cmdlen++;
        break;
    }
  }
end:
  w = grab_str(cmdlen);
  append_wf(&head, &tail, w, cmdlen, (cctx == M_DQUOTE) ? QCMDSUB_DQ : QCMDSUB);
  if ((c = eatbnl()) == SHEOF) {
    if (cctx != M_NORMAL)
      notclosed = 1;
    return SHEOF;
  }
  shungetc(c);
  return 0;
}

static int
lexarith(void)
{
  /* $(()) */
  size_t arlen;
  int depth, n, n2, c;
  int arsp = 0;
  static char arbuf[4096];
  struct {
    size_t pos;
    int depth;
  } arstack[MAX_ARITH];

  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  arlen = 0;
  depth = 0;
startarith:
  for (;;) {
    int ch;
    if ((ch = shgetchar()) == SHEOF) {
      notclosed = 1;
      return SHEOF;
    }
    if (ch == '(') {
      depth++;
      if (arlen >= sizeof(arbuf) - 1) {
        shwarn_arg("arithmetic", arbuf, "expression too long");
        return SHEOF;
      }
      arbuf[arlen++] = ch;
    } else if (ch == '$') {
      if ((n = shgetchar()) == '(') {
        if ((n2 = shgetchar()) == '(') {
          if (arsp < MAX_ARITH) {
            arstack[arsp].pos = arlen;
            arstack[arsp].depth = depth;
            arsp++;
          }
          depth = 0;
          goto startarith;
        }
        shungetc(n2);
      }
      shungetc(n);
      arbuf[arlen++] = ch;
    } else if (ch == ')') {
      if (depth > 0) {
        depth--;
        arbuf[arlen++] = ')';
      } else if (arsp > 0) {
        if ((ch = shgetchar()) == ')') {
          size_t start, rlen, inlen;
          u64 val;
          char res[32];
          start = arstack[arsp - 1].pos;
          val = arith_eval(arbuf + start, arlen - start);
          rlen = lltoa(val, res);
          inlen = arlen - start;
          if (rlen != inlen)
            memmove(arbuf + start + rlen,
                    arbuf + start + inlen,
                    arlen - start - inlen);
          memcpy(arbuf + start, res, rlen);
          arlen = start + rlen;
          arsp--;
          depth = arstack[arsp].depth;
        } else {
          shungetc(ch);
          arbuf[arlen++] = ')';
        }
      } else {
        if ((ch = shgetchar()) == ')')
          break;
        shungetc(ch);
        arbuf[arlen++] = ')';
      }
    } else {
      if (arlen >= sizeof(arbuf) - 1) {
        shwarn_arg("arithmetic", arbuf, "expression too long");
        return SHEOF;
      }
      arbuf[arlen++] = ch;
    }
  }
  arbuf[arlen] = '\0';
  char *exprtxt;
  exprtxt = st_strndup(arbuf, arlen);
  append_wf(&head, &tail, exprtxt, arlen, QARITH);
  if ((c = eatbnl()) == SHEOF)
    return SHEOF;
  shungetc(c);
  return 0;
}

static int
lexvbrace(void)
{
  char *w;
  int c, depth = 0;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  size_t nlen = 0;
  stcheck(32);
  for (int ch = shgetchar();; ch = shgetchar()) {
    if (ch == SHEOF) {
      notclosed = 1;
      return SHEOF;
    }
    if (ch == '}') {
      if (!depth)
        break;
      depth--;
      st_putc(ch);
      nlen++;
      continue;
    }
    if (ch == '$') {
      int n = shgetchar();
      if (n == '{')
        depth++;
      if (n != SHEOF)
        shungetc(n);
    }
    st_putc(ch);
    nlen++;
  }
  w = grab_str(nlen);
  append_wf(&head, &tail, w, nlen, cctx == M_DQUOTE ? QBRACE_DQ : QBRACE);
  if ((c = eatbnl()) == SHEOF)
    return SHEOF;
  shungetc(c);
  return 0;
}

static int
lexvar(int c)
{
  char *w;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  stcheck(32), st_putc(c);
  size_t nlen = 1;
  for (;;) {
    int ch = eatbnl();
    if (!isalnum_(ch) && ch != '_') {
      shungetc(ch);
      break;
    }
    st_putc(ch);
    nlen++;
  }
  w = grab_str(nlen);
  append_wf(&head, &tail, w, nlen, cctx == M_DQUOTE ? QVAR_DQ : QVAR);
  if ((c = eatbnl()) == SHEOF)
    return SHEOF;
  shungetc(c);
  return 0;
}

static int
lexvspecial(int c)
{
  char *w;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  stcheck(32), st_putc(c);
  w = grab_str(1);
  append_wf(&head, &tail, w, 1, cctx == M_DQUOTE ? QVAR_DQ : QVAR);
  if ((c = eatbnl()) == SHEOF)
    return SHEOF;
  shungetc(c);
  return 0;
}

static int
lexvnum(int c)
{
  char *w;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  stcheck(32), st_putc(c);
  size_t nlen = 1;
  for (;;) {
    int ch = eatbnl();
    if (!isdigit_(ch)) {
      shungetc(ch);
      break;
    }
    st_putc(ch);
    nlen++;
  }
  w = grab_str(nlen);
  append_wf(&head, &tail, w, nlen, cctx == M_DQUOTE ? QVAR_DQ : QVAR);
  if ((c = eatbnl()) == SHEOF)
    return SHEOF;
  shungetc(c);
  return 0;
}

static void
lexdquote(void)
{
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  if (cctx == M_DQUOTE)
    popctx();
  else
    pshctx(M_DQUOTE);
}

static void
lexsquote(void)
{
  flushword((cctx == M_SQUOTE) ? QSINGLE : QNONE);
  if (cctx == M_SQUOTE)
    popctx();
  else
    pshctx(M_SQUOTE);
}

static int
lexbtick(void)
{
  /* `cmd`*/
  enum qs cstate;
  size_t cmdlen;
  int c;
  char *w;

  cstate = 0;
  cmdlen = 0;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  stcheck(32);
  for (int ch = shgetchar();; ch = shgetchar()) {
    if (ch == SHEOF) {
      notclosed = 1;
      return SHEOF;
    }
    switch (cstate) {
      qescape(ch)
    }
    switch (ch) {
      case '\'':
        cstate |= insq;
        st_putc(ch);
        cmdlen++;
        break;
      case '"':
        st_putc(ch);
        cmdlen++;
        cstate |= indq;
        break;
      case '\\':
        cstate |= esc;
        st_putc(ch);
        cmdlen++;
        break;
      case '`':
        goto cmdsubend;
      default:
        st_putc(ch);
        cmdlen++;
        break;
    }
  }
cmdsubend:
  w = grab_str(cmdlen);
  append_wf(&head, &tail, w, cmdlen, (cctx == M_DQUOTE) ? QCMDSUB_DQ : QCMDSUB);
  if ((c = eatbnl()) == SHEOF) {
    if (cctx != M_NORMAL)
      notclosed = 1;
    return SHEOF;
  }
  shungetc(c);
  return 0;
}

static void
skipcomment(void)
{
  const char *buf;
  size_t avail;
  for (;;) {
    size_t pos;
    avail = shpeek(&buf);
    if (!avail)
      break;
    pos = sscndelim(buf, avail, "\n", 1);
    if (pos > 0)
      shadvance(pos);
    if (pos < avail)
      break;
  }
}

/* NOLINTEND(readability-function-cognitive-complexity) */
