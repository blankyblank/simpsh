/* lex.c - tokenizer functions */
/* NOLINTBEGIN(readability-function-cognitive-complexity) */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "arith.h"
#include "env.h"
#include "error.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "simd.h"
#include "utils.h"

wf *wf_chunk = NULL;
size_t wf_chunk_left = 0;
int alias_depth = 0;
int notclosed = 0;
sh_tok last_tok = { .type = TNONE };
int chkwd = 0;
#define CTX_MAX 8
#define NCHR(c) (nchars[(unsigned char)c])
#define DCHR(c) (dqchars[(unsigned char)c])
#define SCHR(c) (sqchars[(unsigned char)c])
#define current_ctx (ctx_stack[ctx_depth])
#define push_ctx(m) (ctx_stack[++ctx_depth] = (m))
#define pop_ctx() (ctx_depth--)

#define KEYW(f, s, t) \
  if (memcmp(s, f->word, f->len) == 0) \
    return (sh_tok) { .type = t, .cmd = f }

#define flushword(h, t, w, l, qs) \
  do { \
    if (l > 0) { \
      w = grab_str(l); \
      append_wf(h, t, w, l, qs); \
      l = 0; \
    } \
  } while (0)

typedef enum {
  M_NORMAL,
  M_DQUOTE,
  M_SQUOTE
} tokmode;

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
}; /* everything else is 0 = C_WORD */  /* clang-format on */

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

static wf *get_wf(int);


#define qescape(c) \
  case insq: \
    if (c == '\'') { \
      cstate &= ~insq; \
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
    cstate &= ~esc; \
    continue; \
  case indq: \
    if (c == '"') { \
      cstate &= ~indq; \
      st_putc(c); \
      cmdlen++; \
      continue; \
    } else if (c == '\\') { \
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

  while ((c = shgetchar()) == '\\') {
    if ((n = shgetchar()) == '\n') {
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

 if (f && !f->next && f->word) {
    char *buf = st_alloc(f->len + 1);
    memcpy(buf, f->word, f->len);
    buf[f->len] = '\0';
    return buf;
  }

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
  enum {
    insq = (1 << 0),
    indq = (1 << 1),
    esc = (1 << 2),
  };

  static int ctx_depth;
  static tokmode ctx_stack[CTX_MAX] = { M_NORMAL };
  char n, n2, *w;
  size_t len,  cmdlen;
  wf *head = NULL;
  wf *tail = NULL;
  wf_chunk = NULL;
  wf_chunk_left = 0;
  len = 0;

  for (;;) {
    if (current_ctx == M_NORMAL && nchars[(unsigned char)c] == C_WORD) {
      size_t avail;
      const char *buf;
      size_t pos;

      if ((avail = shpeek(&buf)) >= 16) {
        if ((pos = sscnword(buf, avail)) > 0) {
          if (stleft < pos)
            grow_stack(pos);
          st_putc(c);
          len++;
          memcpy(stnext, buf, pos);
          stnext += pos, stleft -= pos;
          len += pos;
          shadvance(pos);
          if ((c = eatbnl()) == SHEOF)
            goto done;
          continue;
        }
      }
    }
    switch (ctx_tables[current_ctx][(unsigned char)c]) {
      case C_SQUOTE:
        flushword(&head, &tail, w, len,
                  current_ctx == M_SQUOTE ? QSINGLE : QNONE);
        if (current_ctx == M_SQUOTE)
          pop_ctx();
        else
          push_ctx(M_SQUOTE);
        break;

      case C_DQUOTE:
        flushword(&head, &tail, w, len,
                  current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
        if (current_ctx == M_DQUOTE)
          pop_ctx();
        else
          push_ctx(M_DQUOTE);
        break;

      case C_BSLASH:
        if ((n = shgetchar()) == '\n') {
          break;
        }
        if (n == SHEOF)
          goto done;
        flushword(&head, &tail, w, len,
                  current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
        if (current_ctx == M_DQUOTE && n != '$' && n != '"' && n != '\\' &&
            n != '`') {
          st_putc(c);
          len++;
        }
        st_putc(n);
        len++;
        w = grab_str(len);
        append_wf(&head, &tail, w, len, QSINGLE);
        len = 0;
        break;

      case C_DOLLAR:
        n = eatbnl();
        if (n == '(') {
          n2 = eatbnl();
          if (n2 == '(') {
            /* $(()) */
            static char arbuf[4096];
            size_t arlen;
            int depth;
            struct {
              size_t pos;
              int depth;
            } arstack[MAX_ARITH];
            int arsp = 0;

            flushword(&head, &tail, w, len,
                      current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
            arlen = 0;
            depth = 0;
          startarith:
            for (;;) {
              char ch;
              if ((ch = shgetchar()) == SHEOF) {
                notclosed = 1;
                goto done;
              }
              if (ch == '(') {
                depth++;
                if (arlen >= sizeof(arbuf) - 1) {
                  shwarn_arg("arithmetic", arbuf, "expression too long");
                  goto done;  // or break
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
                    long long val;
                    char res[32];
                    start = arstack[arsp - 1].pos;
                    val = arith_eval(arbuf + start, arlen - start);
                    rlen = lltoa(val, res);
                    inlen = arlen - start;
                    if (rlen != inlen)
                      memmove(arbuf + start + rlen, arbuf + start + inlen,
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
                  goto done;  // or break
                }
                arbuf[arlen++] = ch;
              }
            }
            arbuf[arlen] = '\0';
            char *exprtxt;
            exprtxt = st_strndup(arbuf, arlen);
            append_wf(&head, &tail, exprtxt, arlen, QARITH);
            if ((c = eatbnl()) == SHEOF)
              goto done;
            continue;
          } else {
            shungetc(n2);
          }

          /* $() */
          int cstate;
          size_t cmdsubd = 1;
          cstate = 0;
          cmdlen = 0;
          flushword(&head, &tail, w, len,
                    current_ctx == M_DQUOTE ? QDOUBLE : QNONE);

          for (char ch = shgetchar();; ch = shgetchar()) {
            if (ch == SHEOF) {
              notclosed = 1;
              goto done;
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
              case '(':
                st_putc(ch);
                cmdlen++;
                cmdsubd++;
                break;
              case ')':
                cmdsubd--;
                if (!cmdsubd) {
                  goto cmdsubend;
                }
                st_putc(ch);
                cmdlen++;
                break;
              default:
                st_putc(ch);
                cmdlen++;
                break;
            }
          }

          /* ${...} */
        } else if (n == '{') {
          flushword(&head, &tail, w, len,
                    current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
          size_t nlen = 0;
          for (char ch = shgetchar();; ch = shgetchar()) {
            if (ch == SHEOF) {
              notclosed = 1;
              goto done;
            }
            if (ch == '}')
              break;
            st_putc(ch);
            nlen++;
          }
          w = grab_str(nlen);
          append_wf(&head, &tail, w, nlen,
                    current_ctx == M_DQUOTE ? QBRACE_DQ : QBRACE);
          if ((c = eatbnl()) == SHEOF)
            goto done;
          continue;

          /* $name */
        } else if (isalpha_(n) || n == '_') {
          flushword(&head, &tail, w, len,
                    current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
          st_putc(n);
          size_t nlen = 1;
          for (;;) {
            char ch = eatbnl();
            if (!isalnum_(ch) && ch != '_') {
              shungetc(ch);
              break;
            }
            st_putc(ch);
            nlen++;
          }
          w = grab_str(nlen);
          append_wf(&head, &tail, w, nlen,
                    current_ctx == M_DQUOTE ? QVAR_DQ : QVAR);
          if ((c = eatbnl()) == SHEOF)
            goto done;
          continue;

          /* $$ $? $! $# */
        } else if (n == '$' || n == '?' || n == '!' || n == '#') {
          flushword(&head, &tail, w, len,
                    current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
          st_putc(n);
          w = grab_str(1);
          append_wf(&head, &tail, w, 1,
                    current_ctx == M_DQUOTE ? QVAR_DQ : QVAR);
          if ((c = eatbnl()) == SHEOF)
            goto done;
          continue;

          /* $1 numbers */
        } else if (isdigit_(n)) {
          flushword(&head, &tail, w, len,
                    current_ctx == M_DQUOTE ? QBRACE_DQ : QBRACE);
          st_putc(n);
          size_t nlen = 1;
          for (;;) {
            char ch = eatbnl();
            if (!isdigit_(ch)) {
              shungetc(ch);
              break;
            }
            st_putc(ch);
            nlen++;
          }
          w = grab_str(nlen);
          append_wf(&head, &tail, w, nlen,
                    current_ctx == M_DQUOTE ? QVAR_DQ : QVAR);
          if ((c = eatbnl()) == SHEOF)
            goto done;
          continue;

        } else {
          st_putc(c);
          len++;
          shungetc(n);
        }
        break;

      case C_BTICK:
        {
          /* `cmd`*/
          int cstate;

          cstate = 0;
          cmdlen = 0;
          flushword(&head, &tail, w, len,
                    current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
          for (char ch = shgetchar();; ch = shgetchar()) {
            if (ch == SHEOF) {
              notclosed = 1;
              goto done;
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
          append_wf(&head, &tail, w, cmdlen,
                    (current_ctx == M_DQUOTE) ? QCMDSUB_DQ : QCMDSUB);
          if ((c = eatbnl()) == SHEOF) {
            if (current_ctx != M_NORMAL)
              notclosed = 1;
            goto done;
          }
          shungetc(c);
          break;
        }

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
        if (current_ctx == M_NORMAL) {
          shungetc(c);
          goto done;
        }
      /* falls through */
      default:
      case C_WORD:
      case C_COMMENT:
        st_putc(c);
        len++;
        break;
    }

    c = (current_ctx == M_SQUOTE) ? shgetchar() : eatbnl();
    if (c == SHEOF) {
      if (current_ctx != M_NORMAL)
        notclosed = 1;
      goto done;
    }
  }
done:
  /*  one last save  */
  if (len) {
    w = grab_str(len);
    append_wf(&head, &tail, w, len,
              current_ctx == M_SQUOTE ? QSINGLE :
              current_ctx == M_DQUOTE ? QDOUBLE : QNONE);
  }
  return head;
}

/** create sh_toks out of line */
__attribute__((hot)) sh_tok
tokenize(void)
{
  sh_tok t;
  int wd, c, n;
  char *word;
  alias *a;
  wf *f;

  if (last_tok.type != TNONE) {
    t = last_tok;
    last_tok = SHTOK(TNONE);
    return t;
  }
  wd = chkwd;
  chkwd = 0;

  while ((c = shgetchar()) != SHEOF) {
    switch (NCHR(c)) {
      case C_SPACE:
        {
          const char *buf;
          size_t avail = shpeek(&buf);
          if (avail >= 16) {
            size_t skip = sskipspace(buf, avail);
            if (skip > 0)
              shadvance(skip);
          }
        }
        continue;
      case C_COMMENT:
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
        continue;

      case C_NL:
        if (wd & CHKNL)
          continue;
        {
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
        }
        c = shgetchar();
        if (c != '\n' && c != SHEOF)
          shungetc(c);
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
        return SHTOK(TLB);
      case C_RB:
        return SHTOK(TRB);

      case C_LT:
        n = eatbnl();
        if (n == '<') {
          if ((n = eatbnl()) == '-')
            return SHREDIR(RDHERE_D);
          shungetc(n);
          return SHREDIR(RDHERE);
        } else if (n == '&') {
          return SHREDIR(RDDUPI);
        } else if (n == '>') {
          return SHREDIR(RDRW);
        } else {
          shungetc(n);
          return SHREDIR(RDIN);
        }
      case C_GT:
        n = eatbnl();
        if (n == '>') {
          return SHREDIR(RDAPP);
        } else if (n == '&') {
          return SHREDIR(RDDUPO);
        } else if (n == '|') {
          return SHREDIR(RDCLOB);
        } else {
          shungetc(n);
          return SHREDIR(RDOUT);
        }

      case C_BSLASH:
        if ((n = shgetchar()) == '\n')
          continue;
        if (n != SHEOF)
          shungetc(n);
      /* falls through */
      default:
        f = get_wf(c);
        if (!f)
          return SHTOK(TEOF);

        int allnum; // AHEAD OF TIME SCAN
        f->flags = 0;
        if (f->qs == QNONE && !f->next)
          f->flags |= WFSINGLE;
        allnum = 1;
        for (wf *p = f; p; p = p->next) {
          if (p->qs != QNONE)
            f->flags |= WFDOUBLE;
          if (p->qs == QCMDSUB || p->qs == QCMDSUB_DQ)
            f->flags |= WFCMDSUB;
          for (size_t i = 0; i < p->len; i++)
            if (p->word[i] < '0' || p->word[i] > '9')
              allnum = 0;
        }
        if (allnum)
          f->flags |= WFALLNUM;

        if (wd & CHKKWD && (f->flags & WFSINGLE)) {
          switch (f->len) {
            case 1:
              switch (f->word[0]) {
                case '!':
                  KEYW(f, "!", TNOT);
                  break;
              }
              break;
            case 2:
              switch (f->word[0]) {
                case 'd':
                  KEYW(f, "do", TDO);
                  break;
                case 'i':
                  KEYW(f, "if", TIF);
                  KEYW(f, "in", TIN);
                  break;
                case 'f':
                  KEYW(f, "fi", TFI);
                  break;
              }
            break;
            case 3:
              switch (f->word[0]) {
                case 'f':
                  KEYW(f, "for", TFOR);
                  break;
              }
            break;
            case 4:
              switch (f->word[0]) {
                case 'c':
                  KEYW(f, "case", TCASE);
                  break;
                case 'e':
                  KEYW(f, "else", TELSE);
                  KEYW(f, "elif", TELIF);
                  KEYW(f, "esac", TESAC);
                  break;
                case 't':
                  KEYW(f, "then", TTHEN);
                  break;
                case 'd':
                  KEYW(f, "done", TDONE);
                  break;
              }
            break;
            case 5:
              switch (f->word[0]) {
                case 'w':
                  KEYW(f, "while", TWHILE);
                  break;
                case 'u':
                  KEYW(f, "until", TUNTIL);
                  break;
              }
            break;
          }
        }
        if ((wd & CHKALIAS) && (f->flags & WFSINGLE)) {
          word = join_wf(f);
          a = findalias(word);
          if (a) {
            if (alias_depth >= MAX_ALIAS_DEPTH) {
              fprintf(stderr, "alias: too many levels of recursion\n");
              return SHTOK(TEOF);
            }
            pushstring(a->value, strlen(a->value), 1);
            wd &= ~CHKALIAS;
            continue;
          }
        }
        return SHWORD(f);
    }
  }
  return SHTOK(TEOF);
}
/* NOLINTEND(readability-function-cognitive-complexity) */
