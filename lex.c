/* lex.c - tokenizer and parser functions */
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <ctype.h>

#include "lex.h"
#include "env.h"
#include "utils.h"
#include "malloc.h"
#include "input.h"

#define SKIPNL (1 << 0)
#define SEP (1 << 1)
#define XPND (1 << 2)

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
  // ['$'] = C_DOLLAR,

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
static redir *heredoc_head;
static redir **heredoc_tail = &heredoc_head;

static char *join_wf(wf *wordf);
static cmd_tree *parse_andor(void);
static cmd_tree *parse_pipe(void);
static cmd_tree *parse_group(void);
static cmd_tree *parse_func(void);
static void parse_heredoc(void);
static int is_assn(wf *);
static void append_wf(wf **, wf **, char *, size_t len, int);
static wf **get_assn(wf **, char ***);

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

static inline cmd_tree *
newcmdnode(wf **args, int flags, char **sh_vars)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = CMD;
  CARGS(n) = args;
  CVARS(n) = sh_vars;
  n->flags = flags;
  /* cmd tree has no children, and doesn't have an operator in it */
  return n;
}

static inline cmd_tree *
newsubsh(cmd_tree *left, int negate)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = SUBSHELL;
  n->left = left;
  n->flags = negate;
  return n;
}

static inline cmd_tree *
newfuncnode(wf *name, cmd_tree *body)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = FUNC;
  CFUNC(n) = name;
  n->left = body;
  /* redir everything else is empty */
  return n;
}

static inline cmd_tree *
newredirnode(cmd_tree *l, redir *r)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n) {
    perror("st_alloc failed");
    return NULL;
  }
  n->type = REDIR;
  n->left = l;
  CREDR(n) = r;
  n->flags = 0;
  /* redir everything else is empty */
  return n;
}

static inline cmd_tree *
newoppnode(token opp_t, cmd_tree *left, cmd_tree *right)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = OP;
  COPP(n) = opp_t;
  n->left = left;
  n->right = right;
  n->flags = 0;
  /* opp tree doesn't have a command stored and doesn't use the ! opperator */
  return n;
}

/* check if word is name=value */
static int
is_assn(wf *cmd)
{
  char *eq = memchr(cmd->word, '=', cmd->len);
  const char *p = cmd->word;

  if (cmd->qs != QNONE)
    return 0;
  if (!eq || eq == cmd->word)
    return 0;

  for (p = cmd->word; p < eq; p++) {
    if (p == cmd->word) {
      if (!isalpha(*p) && *p != '_')
        return 0;
    } else {
      if (!isalnum(*p) && *p != '_')
        return 0;
    }
  }
  return 1;
}

/** get name and value from NAME=value pair */
wf **
get_assn(wf **args, char ***sh_vars)
{
  int i, j, k, a_c;
  *sh_vars = NULL;

  if (!args)
    return NULL;

  for (i = 0; args[i]; i++)
    if (!is_assn(args[i]))
      break;

  a_c = i;
  if (!a_c) {
    *sh_vars = NULL;
    return args;
  }
  *sh_vars = st_alloc((a_c + 1) * sizeof(char *));
  if (!*sh_vars)
    return NULL;

  for (j = 0; j < a_c; j++)
    (*sh_vars)[j] = join_wf(args[j]);
  (*sh_vars)[a_c] = NULL;

  for (k = a_c; args[k]; k++)
    args[k - a_c] = args[k];
  args[k - a_c] = NULL;

  return args;
}

/* takes word fragment and returns (char *) */
static char *
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
    s = s_mempcpy(s, f->word, f->len);
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

__attribute__((hot)) wf *
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
          n = eatbnl();
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
          n = eatbnl();
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
        while ((c = shgetchar()) != '\n' && c != SHEOF)
          ;
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
            // actually do yet. idk don't like it. whatever
            continue;
          }
        }
        return SHWORD(f);
    }
  }
  return SHTOK(TEOF);
}

__attribute__((hot)) cmd_tree *
parse_list(token s)
{
  sh_tok t;
  cmd_tree *l, *r;

  t = tokenize();
  if (t.type == TEOF || t.type == TNL) {
    if (t.type != TEOF && t.type != s)
      last_tok = t;
    return NULL;
  }
  last_tok = t;

  l = parse_andor();
  if (!l)
    return NULL;

  for (;;) {
    t = tokenize();
    if (t.type == TSEMI || (t.type == TNL && s != TEOF) || t.type == TBKGRND) {
      chkwd |= CHKALIAS;
      if (heredoc_head)
        parse_heredoc();
      r = parse_cmd();
      if (!r)
        if (t.type == TBKGRND) {
          l = newoppnode(TBKGRND, l, NULL);
          break;
        }
      l = newoppnode(t.type, l, r);
    } else {
      if (heredoc_head)
        parse_heredoc();
      if (t.type == TEOF || t.type == s) {
        if (t.type != TEOF)
          last_tok = t;
      } else {
        last_tok = t;
      }
      break;
    }
  }
  return l;
}

/**  parse and or, or lists  */
static cmd_tree *
parse_andor(void)
{
  cmd_tree *l, *r;
  sh_tok t;

  l = parse_pipe();
  if (!l)
    return NULL;

  for (;;) {
    t = tokenize();
    if (t.type == TAND || t.type == TOR) {
      chkwd |= CHKALIAS | CHKNL;
      r = parse_pipe();
      if (!r)
        return NULL;
      l = newoppnode(t.type, l, r);
    } else {
      last_tok = t;
      break;
    }
  }
  return l;
}

/**  parse pipelines  */
static cmd_tree *
parse_pipe(void)
{
  cmd_tree *l, *r;
  sh_tok t;

  l = parse_cmd();
  if (!l)
    return NULL;

  for (;;) {
    t = tokenize();
    if (t.type == TPIPE) {
      chkwd |= CHKALIAS | CHKNL;
      r = parse_cmd();
      if (!r)
        return NULL;
      l = newoppnode(t.type, l, r);
    } else {
      last_tok = t;
      break;
    }
  }
  return l;
}

static cmd_tree *
parse_group(void)
{
  cmd_tree *body;
  sh_tok t;
  body = parse_list(TRB);
  t = tokenize();
  if (t.type != TRB) {
    last_tok = t;
    return NULL;  // syntax error
  }
  chkwd |= CHKALIAS;
  return body;
}

static cmd_tree *
parse_func(void)
{
  sh_tok t;
  t = tokenize();
  if (t.type == TLB)
    return parse_group();
  if (t.type == TLP) {
    cmd_tree *sub;
    sub = parse_list(TRP);
    t = tokenize();
    if (t.type != TRP)
      return NULL;
    chkwd |= CHKALIAS;
    return sub;
  }
  // Anything else is a syntax error
  last_tok = t;
  return NULL;
}

static inline int
allnum(wf *w)
{
  if (w->len == 0 || w->word[0] < '0' || w->word[0] > '9')
    return 0;
  for (size_t i = 0; i < w->len; i++)
    if (w->word[i] < '0' || w->word[i] > '9')
      return 0;
  return 1;
}

/*
 * parser for heredocs
 */
void
parse_heredoc(void)
{
  if (!heredoc_head)
    return;

  redir *r;
  char *eofv, *bpos;
  char c;
  size_t eofvlen, bodylen;

  while (heredoc_head) {
    bpos = NULL;
    eofvlen = 0;
    r = heredoc_head;
    heredoc_head = heredoc_head->heredoc_next;
    eofv = join_wf(r->name);
    eofvlen = strlen(eofv);
    if (r->type == RDHERE_D) {
      c = shgetchar();
      while (c == '\t')
        c = shgetchar();
      shungetc(c);
    }
    bpos = stnext;

    for (;;) {
      char *lpos;
      size_t llen;
      lpos = stnext;

      for (;;) {
        c = shgetchar();
        if (c == SHEOF) {
          fprintf(stderr, "unexpected EOF while looking for delimiter\n");
          return;
        } else if (c == '\n') {
          break;
        }
        st_putc(c);
      }
      llen = stnext - lpos;
      if (llen == eofvlen && memcmp(lpos, eofv, eofvlen) == 0) {
        stunalloc(lpos);
        break;
      } else {
        st_putc('\n');
        continue;
      }
    }

    bodylen = stnext - bpos;
    r->heredoc = grab_str(bodylen);
  }
  heredoc_tail = &heredoc_head;
}

/**  recursive decent parser  */
__attribute__((hot)) cmd_tree *
parse_cmd(void)
{
  size_t neg, wc, cap;
  char **sh_vars;
  cmd_tree *sub, *body, *l;
  redir *redirs, **tail;
  sh_tok t, close;
  wf **args;

  sh_vars = NULL;
  l = NULL;
  neg = 0;
  wc = 0;
  cap = 8;

  for (;;) {
    t = tokenize();
    if (t.type != TNOT) {
      last_tok = t;
      break;
    }
    chkwd |= CHKALIAS;
    neg++;
  }

  t = tokenize();
  if (t.type == TLP) {
    sub = parse_list(TRP);
    t = tokenize();
    if (t.type != TRP)
      return NULL;
    chkwd |= CHKALIAS;
    return newsubsh(sub, (neg & 1) ? NEG : 0);
  }

  last_tok = t;
  args = st_alloc(8 * sizeof(wf *));
  redirs = NULL;
  tail = &redirs;

  for (;;) {
    int fd;
    t = tokenize();
    switch (t.type) {
      case TREDIR:
        {
          sh_tok name;
          redir *r;
          name = tokenize();
          if (name.type != TWORD)
            return NULL;
          r = st_alloc(sizeof(redir));
          if (t.sub == RDHERE || t.sub == RDHERE_D) {
            r->fd = 0;
          } else {
            r->fd = (t.sub == RDIN || t.sub == RDDUPI || t.sub == RDRW) ? 0 : 1;
          }
          r->type = t.sub;
          r->name = name.cmd;
          r->next = NULL;
          *tail = r;
          if (t.sub == RDHERE || t.sub == RDHERE_D) {
            r->heredoc_next = NULL;
            *heredoc_tail = r;
            heredoc_tail = &r->heredoc_next;
          }
          tail = &r->next;
          continue;
        }
      case TWORD:
        {
          sh_tok n = tokenize();
          if (n.type == TREDIR && allnum(t.cmd)) {
            sh_tok name = tokenize();
            if (name.type != TWORD)
              return NULL;
            redir *r;
            r = st_alloc(sizeof(redir));
            r->type = n.sub;
            r->name = name.cmd;
            r->next = NULL;
            *tail = r;
            tail = &r->next;
            fd = 0;
            for (size_t i = 0; i < t.cmd->len; i++)
              fd = fd * 10 + (t.cmd->word[i] - '0');
            r->fd = fd;
            continue;
          }
          last_tok = n;
          if (wc >= cap) {
            wf **new;
            cap *= 2;
            new = st_alloc(cap * sizeof(wf *));
            memcpy(new, args, wc * sizeof(wf *));
            args = new;
          }
          args[wc++] = t.cmd;
          continue;
        }
      default:
        last_tok = t;
        break;
    }
    break;
  }

  args[wc] = NULL;
  if (!wc && redirs) {
    l = newcmdnode(NULL, (neg & 1) ? NEG : 0, NULL);
    return newredirnode(l, redirs);
  }
  if (!wc && redirs == NULL)
    return NULL;

  if (wc == 1 && last_tok.type == TLP && redirs == NULL) {
    last_tok = SHTOK(TNONE);
    close = tokenize();
    if (close.type == TRP) {
      body = parse_func();
      if (!body)
        return NULL;
      return newfuncnode(args[0], body);
    }
    last_tok = close;
    return NULL;
  }

  args = get_assn(args, &sh_vars);
  l = newcmdnode(args, (neg & 1) ? NEG : 0, sh_vars);
  if (redirs)
    return newredirnode(l, redirs);
  return l;
}
