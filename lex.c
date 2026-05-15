/* lex.c - tokenizer and parser functions */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>

#include "lex.h"
#include "env.h"
#include "utils.h"
#include "malloc.h"
#include "input.h"

#define SKIPNL (1 << 0)
#define SEP (1 << 1)
#define XPND (1 << 2)

static int alias_depth = 0;
int notclosed = 0;

static char *cat_wf(wf *wordf);
static cmd_tree *parse_andor(const sh_tok *tokens, size_t cnt, token s, size_t *i);
static cmd_tree *parse_pipe(const sh_tok *tokens, size_t cnt, token s, size_t *i);
static int eatbnl(void);
static int is_assn(wf *);
static void append_wf(wf **, wf **, char *, size_t len, int);
static void expand_alias(char *val, sh_tok *tokens, size_t *c);
static wf **get_argv(const sh_tok *, size_t, size_t *);
static wf **get_assn(wf **, char ***);

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
get_argv(const sh_tok *tokens, size_t cnt, size_t *i)
{
  size_t t = *i;
  size_t j = 0;
  size_t wc = 0;

  while (t < cnt && tokens[t].type == TWORD) {
    /* find how many wf's we need */
    t++;
    wc++;
  }
  wf **argv = st_alloc((wc + 1) * sizeof(wf *));
  t = *i;
  while (t < cnt && tokens[t].type == TWORD) {
    argv[j] = tokens[t].cmd;
    j++;
    t++;
  }
  argv[j] = NULL;
  *i = t;
  return argv;
} /* get_argv */

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

/**  split a line up into tokens store in sh_tok structs  */
sh_tok *
tokenize(int *cnt)
{
  char *word;
  int c, n, state;
  size_t tc = 0;
  wf *f = NULL;
  alias *a = NULL;

  state = (1 << 1) | (1 << 2);
  *cnt = 0;

  sh_tok *tokens = st_alloc(16 * (sizeof(sh_tok)));
  if (!tokens)
    return NULL;

  /* go until we hit the null byte */
  while ((c = shgetchar()) != SHEOF) {
    if (c == ' ' || c == '\t')
      continue;

    /* handle \ nl escape */
    if (c == '\n') {
      if (state & SKIPNL) {
        state &= ~SKIPNL;
        continue;
      }
      if (state & SEP)
        continue;
      tokens[tc].type = TNL;
      tokens[tc].cmd = NULL;
      tc++;
      state |= SEP;
      continue;
    }

    if (c == '#' && (state & XPND)) { /* handle comments */
      while ((c = shgetchar()) != '\n' && c != SHEOF);
      if (c == '\n')
        shungetc(c);
      continue;
    }

    /* update c (count) and p (position) by the length of the operator */
    if (c == '!') {
      tokens[tc].type = TNOT;
      tokens[tc++].cmd = NULL;
      state |= XPND;
      continue;
    } else if (c == '&') {
      n = eatbnl();
      if (n == '&') {
        state |= XPND | SKIPNL;
      } else {
        shungetc(n);
      }
      tokens[tc].type = TAND;
      tokens[tc].cmd = NULL;
      tc++;
      state |= XPND;
      continue;
    } else if (c == '|') {
      n = eatbnl();
      if (n == '|') {
        tokens[tc].type = TOR;
        tokens[tc].cmd = NULL;
        tc++;
        state |= XPND | SKIPNL;
      } else {
        shungetc(n);
        tokens[tc].type = TPIPE;
        tokens[tc].cmd = NULL;
        tc++;
        state |= XPND | SKIPNL;
      }
      continue;
    } else if (c == ';') {
      tokens[tc].type = TSEMI;
      tokens[tc].cmd = NULL;
      tc++;
      state |= XPND;
      continue;
    } else if (c == '(') {
      tokens[tc].type = TLPAREN;
      tokens[tc].cmd = NULL;
      tc++;
      state |= XPND;
      continue;
    } else if (c == ')') {
      tokens[tc].type = TRPAREN;
      tokens[tc].cmd = NULL;
      tc++;
      state |= XPND;
      continue;
    } else {
      if (!(f = get_wf(c)))
        return NULL;
      if (f && f->qs == QNONE && f->next == NULL) {
        word = cat_wf(f);
        if (state & XPND)
          a = find_alias(word);
        if (a) {
          if (alias_depth >= MAX_ALIAS_DEPTH) {
            fprintf(stderr, "alias: too many levels of recursion\n");
            return NULL;
          }
          alias_depth++;
          expand_alias(a->value, tokens, &tc);
          alias_depth--;
          continue;
        }
      }
      if (f->word && f->len > 0) {
        tokens[tc].type = TWORD;
        tokens[tc].cmd = f;
        tc++;
        state &= ~XPND;
      }
    }
  }
  if (tc > 0 && (tokens[tc-1].type == TAND || tokens[tc-1].type == TOR || tokens[tc-1].type == TSEMI))
    notclosed = 1;
  tokens[tc].type = TEOF;
  tokens[tc].cmd = NULL;

  *cnt = tc;
  return tokens;
}


/** literally the same pointless wrapper I already had */
cmd_tree *
build_tree(const sh_tok *tokens, size_t cnt, token stp) {
  size_t i = 0;
  return parse_list(tokens, cnt, stp, &i);
}


/**  parse ; lists  */
cmd_tree *
parse_list(const sh_tok *tokens, size_t cnt, token stp, size_t *i) {
  cmd_tree *left = parse_andor(tokens, cnt, stp, i);
  if (!left)
    return NULL;

  while (*i < cnt && (tokens[*i].type == TSEMI || tokens[*i].type == TNL)) {
    token op = tokens[*i].type;
    (*i)++;
    cmd_tree *right = parse_cmd(tokens, cnt, stp, i);
    if (!right)
      break;
    left = newoppnode(op, left, right);
  }
  return left;
}

/**  parse and or, or lists  */
static cmd_tree *
parse_andor(const sh_tok *tokens, size_t cnt, token stp, size_t *i) {
  cmd_tree *left = parse_pipe(tokens, cnt, stp, i);
  if (!left)
    return NULL;

  while (*i < cnt && (tokens[*i].type == TAND || tokens[*i].type == TOR)) {
    token op = tokens[*i].type;
    (*i)++;
    cmd_tree *right = parse_cmd(tokens, cnt, stp, i);
    if (!right)
      return NULL;
    left = newoppnode(op, left, right);
  }
  return left;
}

/**  parse pipelines  */
static cmd_tree *
parse_pipe(const sh_tok *tokens, size_t cnt, token stp, size_t *i) {
  cmd_tree *left = parse_cmd(tokens, cnt, stp, i);
  if (!left)
    return NULL;
  while (*i < cnt && (tokens[*i].type == TPIPE)) {
    token op = tokens[*i].type;
    (*i)++;
    cmd_tree *right = parse_cmd(tokens, cnt, stp, i);
    if (!right)
      return NULL;
    left = newoppnode(op, left, right);
  }
  return left;
}

/**  recursive decent parser  */
cmd_tree *
parse_cmd(const sh_tok *tokens, size_t cnt, token s, size_t *i)
{
  size_t neg = 0;
  char **sh_vars = NULL;
  wf **args = NULL;
  cmd_false negate;
  cmd_tree *sub, *left = NULL;
  size_t rcnt = cnt;

  for (; *i < cnt && tokens[*i].type == TNOT; (*i)++)
    neg++;
  negate = (neg & 1) ? TRUE : FALSE;
  neg = 0;

  if (*i >= cnt || tokens[*i].type == s) /* check for command */
    return NULL;

  /* if () are found run in SUBSHELL */
  if (tokens[*i].type == TLPAREN) {
    (*i)++;
    rcnt -= *i;
    cmd_tree *tmp = parse_list(tokens, rcnt, TRPAREN, i);
    if (!tmp)
      return NULL;
    if (*i >= cnt || tokens[*i].type != TRPAREN)
      return NULL;
    (*i)++;
    sub = newsubsh(tmp, negate);
    return sub;
  }

  args = get_argv(tokens, cnt, i);
  if (!args || !args[0])
    return NULL;
  args = get_assn(args, &sh_vars);
  left = newcmdnode(args, negate, sh_vars);

  return left;
}

/*  expand alias and handles recursive aliases  */
static void
expand_alias(char *val, sh_tok *tokens, size_t *c)
{
  int ch;
  unsigned int expand;
  char *word;
  wf *f;
  alias *a;

  a = NULL;
  expand = 1;
  setinputstrn(val, strlen(val));

  while ((ch = shgetchar()) != SHEOF) {
    if (ch == ' ' || ch == '\t') {
      continue;
    }
    if (is_operator(ch)) {
      expand = 1;
      break;
    }

    f = get_wf(ch);
    if (!f)
      break;

    if (f->qs == QNONE && f->next == NULL) {
      if (expand) {
        word = cat_wf(f);
        a = find_alias(word);
        if (a) {
          if (alias_depth >= MAX_ALIAS_DEPTH) {
            fprintf(stderr, "alias too many levels of recursion\n");
            popinput();
            return;
          }
          alias_depth++;
          expand_alias(a->value, tokens, c);
          alias_depth--;
          expand = 0;
          continue;
        }
      }
    }
    expand = 0;
    if (f->word && f->len > 0) {
      tokens[*c].type = TWORD;
      tokens[*c].cmd = f;
      (*c)++;
      expand = 0;
    }
  }
  popinput();
}
