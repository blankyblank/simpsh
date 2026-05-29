/* lex.c - tokenizer functions */
#define _POSIX_C_SOURCE 200809L
#include <string.h>

#include "lex.h"
#include "env.h"
#include "utils.h"
#include "malloc.h"
#include "input.h"

static const unsigned char nchars[256] = {
  [' '] = C_SPACE,
  ['\t'] = C_SPACE,
  ['\n'] = C_NL,
  ['#'] = C_COMMENT,
  ['\''] = C_SQUOTE,
  ['"'] = C_DQUOTE,
  ['\\'] = C_BSLASH,
  ['!'] = C_EXCL,
  ['&'] = C_AMP,
  ['|'] = C_PIPE,
  [';'] = C_SEMI,
  ['('] = C_LP,
  [')'] = C_RP,
  ['{'] = C_LB,
  ['}'] = C_RB,
  ['<'] = C_LT,
  ['>'] = C_GT,
  /* everything else is 0 = C_WORD */
};

static const unsigned char dqchars[256] = {
  ['"'] = C_DQUOTE, ['\\'] = C_BSLASH,
};

static const unsigned char sqchars[256] = {
  ['\''] = C_SQUOTE,
};

int alias_depth = 0;
int func_depth = 0;
int notclosed = 0;
sh_tok last_tok = { .type = TNONE };
int chkwd = 0;

static void append_wf(wf **, wf **, char *, size_t len, int);
static wf *get_wf(int);

static inline int
eatbnl(void)
{
  char c, n;

  while ((c = shgetchar()) == '\\') {
    if ((n = shgetchar()) == '\n') {
      cur_shinpt->linenum++;
      continue;
    }
    if (n != SHEOF) {
      shungetc(n);
      return '\\';
    }
  }
  return c;
}

/* takes word fragment and returns (char *) */
char *
join_wf(wf *wordf)
{
  wf *f;
  char *s, *buf;
  size_t len = 0;

  for (f = wordf; f; f = f->next)
    len += f->len;
  buf = st_alloc(len + 1);
  s = buf;
  for (f = wordf; f; f = f->next) {
    s = mempcpy_(s, f->word, f->len);
    *s = '\0';
  }

  return buf;
}

/**  add word fragment onto the end of the linked list  */
static void
append_wf(wf **head, wf **tail, char *w, size_t len, int quoted)
{
  wf *f = st_alloc(sizeof(wf));
  f->word = w;
  f->len = len;
  f->qs = quoted;
  f->next = NULL;
  if (!*head)
    *head = f;
  else
    (*tail)->next = f;
  *tail = f;
}

/** Get word fragment */
__attribute__((hot)) static wf *
get_wf(int c)
{
  /*
   * parses a line extracts a word, handles quoting, escaping.
   *
   * returns immediately if it's at the position of an
   * opterator tokenize handles finding those. also whitespace etc.
   * it advances the position for the caller, then the caller needs to advance
   * through whitespace, and operators when it handles them.
   * it returns word fragments, a linked list of (char *)s
   * that are used to track the quote state of the line later
   * XXX: verify description is still accurate
   */

  wf *head;
  wf *tail;
  // quoted state;
  char n, *w;
  size_t len;
  const unsigned char *state;

  head = NULL;
  tail = NULL;
  state = nchars;
  len = 0;
  for (;;) {
    switch (state[(unsigned char)c]) {
      case C_SQUOTE:
        if (len > 0) {
          w = grab_str(len); /* save unquoted frag if nonempty */
          append_wf(&head, &tail, w, len, state == sqchars ? QSINGLE : QNONE);
          len = 0;
        }
        state = (state == sqchars) ? nchars : sqchars;
        break;
      case C_DQUOTE:
        if (len > 0) {
          w = grab_str(len); /* save unquoted frag if nonempty */
          append_wf(&head, &tail, w, len, state == dqchars ? QDOUBLE : QNONE);
          len = 0;
        }
        state = (state == dqchars) ? nchars : dqchars;
        break;
      case C_BSLASH:
        if (state == dqchars) {
          n = shgetchar();
          if (n == '\n') {
            cur_shinpt->linenum++;
            break;
          }
          if (n == SHEOF)
            goto done;
          if (n == '$' || n == '"' || n == '\\') {
            st_putc(n);
            len++;
          } else {
            st_putc(c);
            len++;
            shungetc(n);
          }
          break;
        } else {
          n = shgetchar();
          if (n == '\n') {
            cur_shinpt->linenum++;
            break;
          }
          if (n == SHEOF)
            goto done;
          st_putc(n);
          len++;
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
        if (state == nchars) {
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

    c = (state == sqchars) ? shgetchar() : eatbnl();
    if (c == SHEOF) {
      if (state != nchars)
        notclosed = 1;
      goto done;
    }
  }
done:
  /*  one last save  */
  if (len) {
    w = grab_str(len);
    append_wf(&head, &tail, w, len,
              state == sqchars ? QSINGLE :
              state == dqchars ? QDOUBLE : QNONE);
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
    switch (nchars[(unsigned char)c]) {
      case C_SPACE:
        continue;
      case C_COMMENT:
        while ((c = shgetchar()) != '\n' && c != SHEOF) {}
        if (c == '\n')
          shungetc(c);
        continue;
      case C_NL:
        if (wd & CHKNL)
          continue;
        cur_shinpt->linenum++;
        while ((c = shgetchar()) == '\n')
          cur_shinpt->linenum++;
        if (c != SHEOF)
          shungetc(c);
        t = SHTOK(TNL);
        return t;
      case C_EXCL:
        return t = SHTOK(TNOT);
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
        if ((n = shgetchar()) == '\n') {
          cur_shinpt->linenum++;
          continue;
        }
        if (c != SHEOF)
          shungetc(n);
        break;
      default:
        f = get_wf(c);
        if (!f)
          return SHTOK(TEOF);
        if ((wd & CHKALIAS) && f->qs == QNONE && f->next == NULL) {
          word = join_wf(f);
          a = findalias(word);
          if (a) {
            if (alias_depth >= MAX_ALIAS_DEPTH) {
              fprintf(stderr, "alias: too many levels of recursion\n");
              return SHTOK(TEOF);
            }
            pushstring(a->value, strlen(a->value), 1);
            wd &= ~CHKALIAS;
            // push string i guess goes here. idk wtf it's even supposed to
            continue;
          }
        }
        return SHWORD(f);
    }
  }
  return SHTOK(TEOF);
}

