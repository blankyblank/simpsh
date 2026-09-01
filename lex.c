/* lex.c - tokenizer functions */

/* NOLINTBEGIN(readability-function-cognitive-complexity) */
#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "main.h"
#include "env.h"
#include "errmsg.h"
#include "input.h"
#include "lex.h"
#include "simd.h"
#include "utils.h"

wf *wf_chunk;
static wf *head;
static wf *tail;
static int wfredir;
static size_t wflen;
static int btdepth;
unsigned int wf_chunk_left;
int alias_depth;
int notclosed;
int chkwd;

/* current context */
#define cctx      (ctx_stack[ctx_depth])
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

/* clang-format off */
static const unsigned char nchars[256] = {
  [' ']  = C_SPACE,
  ['\t'] = C_SPACE,
  ['\n'] = C_NL,
  ['#']  = C_COMMENT,
  ['\''] = C_SQUOTE,
  ['"']  = C_DQUOTE,
  ['\\'] = C_BSLASH,
  ['$']  = C_DOLLAR,
  ['&']  = C_AMP,
  ['|']  = C_PIPE,
  [';']  = C_SEMI,
  ['(']  = C_LP,
  [')']  = C_RP,
  ['{']  = C_LB,
  ['}']  = C_RB,
  ['<']  = C_LT,
  ['>']  = C_GT,
  ['`']  = C_BTICK,
}; /* everything else is 0 = C_WORD */

static const unsigned char dqchars[256] = {
  ['"'] = C_DQUOTE,
  ['\\'] = C_BSLASH,
  ['$'] = C_DOLLAR,
  ['`'] = C_BTICK,
};

static const unsigned char hdchars[256] = {
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
  [M_BRACE] = nchars,
  [M_HEREDOC] = hdchars,
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

int ctx_depth;
tokmode ctx_stack[CTX_MAX] = { M_NORMAL };

static sh_tok tokword(wf *, int*);
static void tokws(void);
static sh_tok toklt(void);
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
  case esc | indq: \
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
  int c, n;

  while ((c = shgetchar()) == '\\') {
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
join_wf(wf *wordf, int esc)
{
  if (!wordf)
    return st_strndup("", 0);
  wf *f = wordf;
  char *s, *buf;
  size_t len = 0;

  if (esc) {
    for (f = wordf; f && f->word; f = f->next)
      for (size_t i = 0; i < f->len; i++)
        len += (f->qs != QNONE && f->qs != QCMDSUB && (f->word[i] == '*' ||
              f->word[i] == '?' || f->word[i] == '[' || f->word[i] == '\\')) ? 2 : 1;
    buf = st_alloc(len + 1);
    s = buf;
    for (f = wordf; f && f->word; f = f->next)
      for (size_t i = 0; i < f->len; i++) {
        if (f->qs != QNONE && f->qs != QCMDSUB &&
            (f->word[i] == '*' || f->word[i] == '?' ||
             f->word[i] == '[' || f->word[i] == '\\'))
          *s++ = '\\';
        *s++ = f->word[i];
      }
    *s = '\0';
    return buf;
  }

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

// char *
// join_esc

/** Get wf's from input */
__attribute__((hot)) wf *
get_wf(int c)
{
  char *w;
  wf_chunk = NULL;
  wf_chunk_left = 0;
  wflen = 0;
  head = NULL;
  tail = NULL;

  for (;;) {
    if (cctx == M_NORMAL || cctx == M_BRACE) {
      if (stleft < 32)
        grow_stack(32);
      while (nchars[(unsigned char)c] == C_WORD) {
        stcheck(32);
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
        if (btdepth > 0) {
          flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
          shungetc(c);
          goto done;
        }
        c = lexbtick();
        if (c == SHEOF)
          goto done;
        break;

      case C_RB:
        if (cctx == M_BRACE) {
          flushword(QNONE);
          popctx();
          append_wf(&head, &tail, grab_str(0), 0, QBRACE_END);
          break;
        }
        if (cctx == M_NORMAL) {
          st_putc(c);
          wflen++;
          break;
        }
        /* falls through */
      case C_AMP:
      case C_PIPE:
      case C_SEMI:
      case C_LP:
      case C_RP:
      case C_LT:
      case C_GT:
        if ((c == '<' || c == '>') && wflen > 0 && cctx == M_NORMAL) {
          char *p = stnext - wflen;
          int allnum = 1;
          for (size_t i = 0; i < wflen; i++)
            if (!isdigit_(p[i])) {
              allnum = 0;
              break;
            }
          if (allnum)
            wfredir = 1;
        }
        /* fall through */
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
  } else if (!head && !notclosed) {
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
        if (!heredoc_head) {
          const char *buf;
          size_t avail, skip;
          while ((avail = shpeek(&buf)) > 0) {
            skip = sskipnl(buf, avail);
            if (skip > 0)
              shadvance(skip);
            if (skip < avail)
              break;
          }
        }
        return SHTOK(TNL);
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
        if (wd & CHKBRACE)
          return SHTOK(TLB);
        goto word;
      case C_RB:
        if (wd & CHKBRACE)
          return SHTOK(TRB);
        goto word;
      case C_LT:
        return toklt();
      case C_GT:
        return tokgt();
      case C_BTICK:
        if (btdepth > 0) {
          btdepth--;
          return SHTOK(TBTICK);
        }
        goto word;
      case C_BSLASH:
        if ((n = shgetchar()) == '\n') {
          shinpt->linenum++;
          continue;
        }
        if (n != SHEOF)
          shungetc(n);
        /* falls through */
word:
      default:
        f = get_wf(c);
        if (!f)
          return SHTOK(TEOF);
        sh_tok t = tokword(f, &wd);
        if (t.type == TCONT)
          continue;
        if (wfredir) {
          t.cmd->flags |= WFREDIRFD;
          wfredir = 0;
        }
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
    word = join_wf(f, 0);
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

// static sh_tok
// toknl(void)
// {
//   int c;
//   const char *buf;
//   size_t avail;
//   while ((avail = shpeek(&buf)) > 0) {
//     size_t skip;
//     skip = sskipnl(buf, avail);
//     if (skip > 0)
//       shadvance(skip);
//     if (skip < avail)
//       break;
//   }
//   c = shgetchar();
//   if (c != '\n' && c != SHEOF)
//     shungetc(c);
//   return SHTOK(TNL);
// }

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
  if ((cctx == M_DQUOTE && n != '$' && n != '"' && n != '\\' && n != '`') ||
      (cctx == M_HEREDOC && n != '$' && n != '\\' && n != '`')) {
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
  wf *svhead, *svtail, *f;
  size_t svwflen;
  int svctx, svbt, svcctx, svlinenum;
  cmd_tree *n;

  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);

  svhead = head;
  svtail = tail;
  svwflen = wflen;
  svctx = ctx_depth;
  svcctx = cctx;
  svbt = btdepth;
  svlinenum = shinpt->linenum;

  head = NULL;
  tail = NULL;
  wflen = 0;
  ctx_depth = btdepth = 0;

  n = parse_list(1);
  if (tbuf.type != TRP) {
    notclosed = 1;
    head = svhead;
    tail = svtail;
    wflen = svwflen;
    ctx_depth = svctx;
    cctx = svcctx;
    btdepth = svbt;
    shinpt->linenum = svlinenum;
    return SHEOF;
  }

  f = wfalloc();
  f->cmdsub = n;
  f->qs = (svcctx == M_DQUOTE) ? QCMDSUB_DQ : QCMDSUB;
  f->len = 0;
  f->next = NULL;
  f->flags = 0;

  head = svhead;
  tail = svtail;
  wflen = svwflen;
  ctx_depth = svctx;
  cctx = svcctx;
  btdepth = svbt;
  shinpt->linenum = svlinenum;
  if (head)
    tail->next = f;
  else
    head = f;
  tail = f;

  return 0;
}

static int
lexbtick(void)
{
  wf *svhead, *svtail, *f;
  size_t svwflen;
  int svctx, svbt, svcctx, svlinenum;
  cmd_tree *n;

  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);

  svhead = head;
  svtail = tail;
  svwflen = wflen;
  svctx = ctx_depth;
  svcctx = cctx;
  svlinenum = shinpt->linenum;

  head = NULL;
  tail = NULL;
  wflen = 0;
  ctx_depth = 0;

  svbt = btdepth;
  btdepth = 1;
  n = parse_list(1);
  btdepth = svbt;

  if (tbuf.type != TBTICK) {
    notclosed = 1;
    head = svhead;
    tail = svtail;
    wflen = svwflen;
    ctx_depth = svctx;
    cctx = svcctx;
    shinpt->linenum = svlinenum;
    return SHEOF;
  }
  f = wfalloc();
  f->cmdsub = n;
  f->qs = (svcctx == M_DQUOTE) ? QCMDSUB_DQ : QCMDSUB;
  f->len = 0;
  f->next = NULL;
  f->flags = 0;

  head = svhead;
  tail = svtail;
  wflen = svwflen;
  ctx_depth = svctx;
  cctx = svcctx;
  shinpt->linenum = svlinenum;
  if (head)
    tail->next = f;
  else
    head = f;
  tail = f;

  return 0;
}

static int
lexarith(void)
{
  size_t arlen;
  int depth = 0, c;
  static char arbuf[4096];

  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  arlen = 0;
  for (;;) {
    int ch;
    if ((ch = shgetchar()) == SHEOF) {
      notclosed = 1;
      return SHEOF;
    }
    if (ch == '(')
      depth++;
    else if (ch == ')') {
      if (depth > 0)
        depth--;
      else if ((ch = shgetchar()) == ')')
        break;
      else
        shungetc(ch);
    }
    if (arlen >= sizeof(arbuf) - 1) {
      shwarn_arg("arithmetic", arbuf, "expression too long");
      return SHEOF;
    }
    arbuf[arlen++] = ch;
  }
  arbuf[arlen] = '\0';
  char *exprtxt = st_strndup(arbuf, arlen);
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
  int c;
  size_t nlen = 0;
  int hasop = 0, ch;
  flushword((cctx == M_DQUOTE) ? QDOUBLE : QNONE);
  stcheck(32);

  ch = shgetchar();
  if (ch == SHEOF) {
    notclosed = 1;
    return SHEOF;
  }

  /* ${#param} length expansion: header is the whole content */
  if (ch == '#') {
    int depth = 0;
    st_putc(ch);
    nlen++;
    for (;;) {
      ch = shgetchar();
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

  st_putc(ch);
  nlen++;

  /* parameter name */
  if (isdigit_(ch)) {
    for (;;) {
      int n = shgetchar();
      if (n == SHEOF) {
        notclosed = 1;
        return SHEOF;
      }
      if (!isdigit_(n)) {
        shungetc(n);
        break;
      }
      st_putc(n);
      nlen++;
    }
  } else if (isalpha_(ch) || ch == '_') {
    for (;;) {
      int n = shgetchar();
      if (n == SHEOF) {
        notclosed = 1;
        return SHEOF;
      }
      if (!isalnum_(n) && n != '_') {
        shungetc(n);
        break;
      }
      st_putc(n);
      nlen++;
    }
  }

  /* operator at the name boundary */
  c = shgetchar();
  if (c == SHEOF) {
    notclosed = 1;
    return SHEOF;
  }
  if (c == ':') {
    int n = shgetchar();
    if (n == SHEOF) {
      notclosed = 1;
      return SHEOF;
    }
    if (n == '-' || n == '=' || n == '?' || n == '+') {
      st_putc(c);
      st_putc(n);
      nlen += 2;
      hasop = 1;
    } else {
      shungetc(n);
      shungetc(c);
    }
  } else if (c == '-' || c == '=' || c == '?' || c == '+') {
    st_putc(c);
    nlen++;
    hasop = 1;
  } else if (c == '#' || c == '%') {
    int n = shgetchar();
    if (n == c) {
      st_putc(c);
      st_putc(n);
      nlen += 2;
    } else {
      if (n != SHEOF)
        shungetc(n);
      st_putc(c);
      nlen++;
    }
    hasop = 1;
  } else {
    shungetc(c);
  }

  if (hasop) {
    w = grab_str(nlen);
    append_wf(&head, &tail, w, nlen, cctx == M_DQUOTE ? QBRACE_DQ : QBRACE);
    pshctx(M_BRACE);
    return 0;
  }

  /* no operator: rest of the content is the header (old behavior) */
  {
    int depth = 0;
    for (;;) {
      ch = shgetchar();
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

wf *
lex_heredoc(const char *body, size_t len)
{
  wf *f;
  int c;
  setinputstrn((char *)body, len);
  pshctx(M_HEREDOC);
  c = shgetchar();
  if (c == SHEOF) {
    popctx();
    notclosed = 0;
    popinput();
    return NULL;
  }
  f = get_wf(c);
  popctx();
  notclosed = 0;
  popinput();
  return f;
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
