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

int alias_depth = 0;
int notclosed = 0;
sh_tok last_tok = { .type = TNONE };
int chkwd = 0;

static char *cat_wf(wf *wordf);
static cmd_tree *parse_andor(void);
static cmd_tree *parse_pipe(void);
static int eatbnl(void);
static int is_assn(wf *);
static void append_wf(wf **, wf **, char *, size_t len, int);
static wf **get_assn(wf **, char ***);
static wf **get_argv(wf *);

static inline cmd_tree *
newcmdnode(wf **args, cmd_false negate, char **sh_vars)
{
  cmd_tree *ct = st_alloc(sizeof(cmd_tree));
  if (!ct) {
    perror("st_alloc failed");
    return NULL;
  }
  ct->type = CMD;
  ct->args = args;
  ct->negate = negate;
  ct->sh_vars = sh_vars;
  /* cmd tree has no children, and doesn't have an operator in it */
  ct->left = NULL;
  ct->right = NULL;
  ct->op_t = 0;

  return ct;
}

static inline cmd_tree *
newsubsh(cmd_tree *left, cmd_false negate)
{
  cmd_tree *ct = st_alloc(sizeof(cmd_tree));
  if (!ct) {
    perror("st_alloc failed");
    return NULL;
  }
  ct->type = SUBSHELL;
  ct->left = left;
  ct->negate = negate;
  /* creating a subshell node we shouldn't need these */
  ct->args = NULL;
  ct->sh_vars = NULL;
  ct->right = NULL;
  ct->op_t = 0;

  return ct;
}

static inline cmd_tree *
newoppnode(token opp_t, cmd_tree *left, cmd_tree *right)
{
  cmd_tree *ot = st_alloc(sizeof(cmd_tree));
  if (!ot) {
    fprintf(stderr, "st_alloc failed");
    return NULL;
  }
  ot->type = OP;
  ot->op_t = opp_t;
  ot->left = left;
  ot->right = right;
  /* opp tree doesn't have a command stored and doesn't use the ! opperator */
  ot->sh_vars = NULL;
  ot->args = NULL;
  ot->negate = 0;

  return ot;
}

static int
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

/* build consecutive TWORDS into command returned in word fragments */
static wf **
get_argv(wf *f)
{
  size_t cap, wc;
  sh_tok t;
  wf **argv, **new;

  cap = 8;
  wc = 1;
  argv = st_alloc(cap * sizeof(wf *));
  argv[0] = f;

  for (;;) {
    t = tokenize();
    if (t.type != TWORD)
      break;
    if (wc >= cap) {
      cap *= 2;
      new = st_alloc(cap * sizeof(wf *));
      memcpy(new, argv, wc * sizeof(wf *));
      argv = new;
    }
    argv[wc++] = t.cmd;
  }
  last_tok = t;
  argv[wc] = NULL;
  return argv;
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

/* takes word fragment and returns (char *) */
static char *
cat_wf(wf *wordf)
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
    (*sh_vars)[j] = cat_wf(args[j]);
  (*sh_vars)[a_c] = NULL;

  for (k = a_c; args[k]; k++)
    args[k - a_c] = args[k];
  args[k - a_c] = NULL;

  return args;
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

wf *
get_wf(int c)
{
  /*
   * parses a line extracts a word, handles quoting, escaping.
   *
   * used in tokenize, it returns immediately if it's at the position of an
   * opterator tokenize handles finding those. also whitespace etc.
   * it advances the position for the caller, then the caller needs to advance
   * through whitespace, and operators when it handles them.
   *
   * it returns word fragments, they're a linked list for (char *)s
   * that are used to track the quoting state of the line later
   * (maybe other metadate too later, if needed)
   */

  wf *head = NULL;
  wf *tail = NULL;
  quoted state;
  char n, *w;
  size_t len;
  // char *s = p;

  state = QNONE;
  len = 0;
  for (;;) {
    switch (state) {
      case QNONE:
        if (c == '\'') {
          if (len > 0) {
            w = grab_str(len); /* save unquoted frag if nonempty */
            append_wf(&head, &tail, w, len, state);
            len = 0;
          }
          state = QSINGLE;
        } else if (is_operator(c)) {
          shungetc(c);
          goto done;
        } else if (is_cmd_end(c)) {
          shungetc(c);
          goto done;
        } else if (c == '"') {
          if (len > 0) {
            w = grab_str(len); /* save unquoted frag if nonempty */
            append_wf(&head, &tail, w, len, state);
            len = 0;
          }
          state = QDOUBLE;
        } else if (c == '\\') {
          n = eatbnl();
          if (n == SHEOF)
            goto done;
          st_putc(n);
          len++;
        } else {
          st_putc(c);
          len++;
        }
        break;
      case QSINGLE:
        if (c == '\'') {
          if (len > 0) {
            w = grab_str(len); /* save single quoted frag if nonempty */
            append_wf(&head, &tail, w, len, state);
            len = 0;
          }
          state = QNONE;
        } else {
          st_putc(c);
          len++;
        }
        break;
      case QDOUBLE:
        if (c == '"') {
          if (len > 0) {
            w = grab_str(len); /* save double quoted frag if nonempty */
            append_wf(&head, &tail, w, len, state);
            len = 0;
          }
          state = QNONE;
        } else if (c == '\\') {
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
        } else {
          st_putc(c);
          len++;
        }
        break;
    }
    if (state == QSINGLE)
      c = shgetchar();
    else 
      c = eatbnl();
    if (c == SHEOF) {
      if (state != QNONE)
        notclosed = 1;
      goto done;
    }
  }

done:
  /*  one last save  */
  if (len) {
    w = grab_str(len);
    append_wf(&head, &tail, w, len, state);
  }
  return head;
}

sh_tok
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
    if (c == ' ' || c == '\t')
      continue;
    if (c == '#') { /* handle comments */
      while ((c = shgetchar()) != '\n' && c != SHEOF);
      if (c == '\n')
        shungetc(c);
      continue;
    } else if (c == '\n') { /* handle \ nl escape */
      if (wd & CHKNL)
        continue;
      cur_shinpt->linenum++;
      while ((c = shgetchar()) == '\n')
        cur_shinpt->linenum++;
      if (c != SHEOF)
        shungetc(c);
      t = SHTOK(TNL);
      return t;
    } else if (c == '!') {
      return t = SHTOK(TNOT);
    } else if (c == '&') {
      n = eatbnl();
      if (n == '&')
        return SHTOK(TAND);
      if (n != SHEOF)
        shungetc(n);
      return SHTOK(TBKGRND);
    } else if (c == '|') {
      n = eatbnl();
      if (n == '|')
        return SHTOK(TOR);
      if (n != SHEOF)
        shungetc(n);
      return SHTOK(TPIPE);
    } else if (c == ';') {
      return SHTOK(TSEMI);
    } else if (c == '(') {
      return SHTOK(TLPAREN);
    } else if (c == ')') {
      return SHTOK(TRPAREN);
    }

    f = get_wf(c);
    if (!f)
      return SHTOK(TEOF);
    if ((wd & CHKALIAS) && f->qs == QNONE && f->next == NULL) {
      word = cat_wf(f);
      a = find_alias(word);
      if (a) {
        if (alias_depth >= MAX_ALIAS_DEPTH) {
          fprintf(stderr, "alias: too many levels of recursion\n");
          return SHTOK(TEOF);
        }
        pushstring(a->value, strlen(a->value), 1);
        wd &= ~CHKALIAS;
        // push string i guess goes here. idk wtf it's even supposed to actually
        // do yet. idk don't like it. whatever
        continue;
      }
    }
    return SHWORD(f);
  }
  return SHTOK(TEOF);
}

cmd_tree *
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
    if (t.type == TSEMI || (t.type == TNL && s != TEOF)) {
      chkwd |= CHKALIAS;
      r = parse_cmd();
      if (!r)
        break;
      l = newoppnode(t.type, l, r);
    } else if (t.type == TEOF || t.type == s) {
      if (t.type != TEOF)
        last_tok = t;
      break;
    } else {
      last_tok = t;
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

/**  recursive decent parser  */
cmd_tree *
parse_cmd(void)
{
  size_t neg = 0;
  char **sh_vars = NULL;
  wf **args = NULL;
  cmd_tree *sub, *l = NULL;
  sh_tok t;

  for (;;) {
    t = tokenize();
    if (t.type != TNOT) {
      last_tok = t;
      break;
    }
    chkwd |= CHKALIAS;
    neg++;
  }
  // WARN: I'm pretty sure this is going to break command negation. 

  t = tokenize();

  if (t.type == TLPAREN) {
    sub = parse_list(TRPAREN);
    t = tokenize();
    if (t.type != TRPAREN)
      return NULL;
    chkwd |= CHKALIAS;
    return newsubsh(sub, neg & 1);
  }

  if (t.type != TWORD) {
    last_tok = t;
    return NULL;
  }

  args = get_argv(t.cmd);
  if (!args || !args[0])
    return NULL;
  args = get_assn(args, &sh_vars);
  l = newcmdnode(args, (neg & 1), sh_vars);

  return l;
}
