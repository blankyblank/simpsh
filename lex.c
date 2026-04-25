/* lex.c - tokenizer and parser functions */
#include "simpsh.h"
#include "lex.h"
#include "env.h"
#include "utils.h"
#include "malloc.h"
#include <ctype.h>

static wf **get_argv(const sh_tok *, size_t, size_t *);
static wf **get_assn(wf **, char ***);
static void append_wf(wf **, wf **, char *, int);
static int is_assn(wf *);
static char *cat_wf(wf *wordf);
static void expand_alias(char *val, sh_tok *tokens, size_t *c);

static cmd_tree *parse_list(const sh_tok *tokens, size_t cnt, token s, size_t *i);
static cmd_tree *parse_andor(const sh_tok *tokens, size_t cnt, token s, size_t *i);
static cmd_tree *parse_pipe(const sh_tok *tokens, size_t cnt, token s, size_t *i);

static int alias_depth = 0;

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
  char *eq = strchr(cmd->word, '=');
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
} /* is_assn */

/* takes word fragment and returns (char *) */
static char *
cat_wf(wf *wordf)
{
  wf *f = wordf;
  char *s;
  size_t len = 0;

  for (; f; f = f->next) {
    for (s = f->word; *s; s++) {
      st_putc(*s);
      len++;
    }
  }

  return grab_str(len);
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
append_wf(wf **head, wf **tail, char *w, int quoted)
{
  wf *f = st_alloc(sizeof(wf));
  f->word = w;
  f->qs = quoted;
  f->next = NULL;
  if (!*head)
    *head = f;
  else
    (*tail)->next = f;
  *tail = f;
}

/**  parses a line extracts a word, handles quoting, escaping.  */
wf *
get_wf(char *line, size_t *pos)
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
  size_t i = *pos;
  char c, n;
  char *w;
  size_t len = 0;
  // char *s = p;

  state = QNONE;

  while (line[i]) {
    c = line[i];
    n = line[i + 1];
    switch (state) {
    case QNONE:
      if (is_cmd_end(c)) {
        goto done;
      } else if (is_operator(c)) {
        goto done;
      } else if (c == '\'') {
        /* save unquoted frag if nonempty */
        if (len > 0) {
          w = grab_str(len);
          append_wf(&head, &tail, w, state);
          len = 0;
        }
        state = QSINGLE;
        i++;
      } else if (c == '"') {
        /* save unquoted frag if nonempty */
        if (len > 0) {
          w = grab_str(len);
          append_wf(&head, &tail, w, state);
          len = 0;
        }
        state = QDOUBLE;
        i++;
      } else if (c == '\\') {
        if (n == '\0') {
          i++;
          goto done;
        } else {
          // i think I should use n where I already used n
          st_putc(n);
          len++;
          i += 2;
        }
      } else {
        st_putc(c);
        len++;
        i++;
      }
      break;
    case QSINGLE:
      if (c == '\'') {
        /* save single quoted frag if nonempty */
        if (len > 0) {
          w = grab_str(len);
          append_wf(&head, &tail, w, state);
          len = 0;
        }
        state = QNONE;
        i++;
      } else {
        st_putc(c);
        len++;
        i++;
      }
      break;
    case QDOUBLE:
      if (c == '"') {
        /* save double quoted frag if nonempty */
        if (len > 0) {
          w = grab_str(len);
          append_wf(&head, &tail, w, state);
          len = 0;
        }
        state = QNONE;
        i++;
      } else if (c == '\\') {
        if (n == '\n') {
          i += 2;
          continue;
        } else if (n == '$' || n == '"' || n == '\\') {
          st_putc(n);
          len++;
          i += 2;
        } else {
          st_putc(c);
          len++;
          i++;
        }
      } else {
        st_putc(c);
        len++;
        i++;
      }
      break;
    }
  }

done:
  /*  one last save  */
  if (len) {
    w = grab_str(len);
    append_wf(&head, &tail, w, state);
  }
  *pos = i;
  return head;
}

/**  split a line up into tokens store in sh_tok structs  */
sh_tok *
tokenize(char *line, int *cnt)
{
  size_t p = 0, c = 0, n;
  unsigned int expand = 1;
  char *word;
  wf *f = NULL;
  alias *a = NULL;
  *cnt = 0;

  if (!line) {
    *cnt = -1;
    return NULL;
  }
  if (strlen(line) == 0)
    return NULL;

  sh_tok *tokens = st_alloc(16 * (sizeof(sh_tok)));
  p = 0;
  if (!tokens)
    return NULL;

  /* go until we hit the null byte */
  while (line[p]) {
    p = skip_ws(line + p) - line; /* skip whitespace */
    if (!line[p])
      break;

    n = p + 1; /* the next c-string */
    /* update c (count) and p (position) by the length of the operator */
    if (line[p] == '!') {
      tokens[c].type = TNOT;
      tokens[c].cmd = NULL;
      c++;
      p++;
    } else if (line[p] == '&' && line[n] == '&') {
      tokens[c].type = TAND;
      tokens[c].cmd = NULL;
      c++;
      p += 2;
      expand |= 1;
      continue;
    } else if (line[p] == '|') {
      if (line[n] == '|') {
        tokens[c].type = TOR;
        tokens[c].cmd = NULL;
        c++;
        p += 2;
        expand |= 1;
      } else {
        tokens[c].type = TPIPE;
        tokens[c].cmd = NULL;
        c++;
        p++;
        expand |= 1;
      }
      continue;
    } else if (line[p] == ';') {
      tokens[c].type = TSEMI;
      tokens[c].cmd = NULL;
      c++;
      p++;
      expand |= 1;
      continue;
    } else if (line[p] == '(') {
      tokens[c].type = TLPAREN;
      tokens[c].cmd = NULL;
      c++;
      p++;
      expand |= 1;
      continue;
    } else if (line[p] == ')') {
      tokens[c].type = TRPAREN;
      tokens[c].cmd = NULL;
      c++;
      p++;
      expand |= 1;
      continue;
    } else {
      if (!(f = get_wf(line, &p)))
        return NULL;
      if (f && f->qs == QNONE && f->next == NULL) {
        word = cat_wf(f);
        if (expand & 1)
          a = find_alias(word);
        if (a) {
          if (alias_depth >= MAX_ALIAS_DEPTH) {
            fprintf(stderr, "alias: too many levels of recursion\n");
            return NULL;
          }
          alias_depth++;
          expand_alias(a->value, tokens, &c);
          alias_depth--;
          continue;
        }
      }
      if (f->word && f->word[0] != '\0') {
        tokens[c].type = TWORD;
        tokens[c].cmd = f;
        c++;
        expand &= ~1;
      }
    }
  }
  tokens[c].type = TEOF;
  tokens[c].cmd = NULL;

  *cnt = c;
  return tokens;
}


/** literally the same pointless wrapper I already had */
cmd_tree *
build_tree(const sh_tok *tokens, size_t cnt, token stp) {
  size_t i = 0;
  return parse_list(tokens, cnt, stp, &i);
}

/**  parse ; lists  */
static cmd_tree *
parse_list(const sh_tok *tokens, size_t cnt, token stp, size_t *i) {
  cmd_tree *left = parse_andor(tokens, cnt, stp, i);
  if (!left)
    return NULL;

  while (*i < cnt && tokens[*i].type == TSEMI) {
    token op = tokens[*i].type;
    (*i)++;
    cmd_tree *right = parse_cmd(tokens, cnt, stp, i);
    if (!right)
      return NULL;
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
  negate = (neg % 2) ? TRUE : FALSE;
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
  size_t pos = 0;
  unsigned int expand = 1;
  char *word;
  wf *f;
  alias *a = NULL;

  while (val[pos]) {
    pos = skip_ws(val + pos) - val;
    if (!val[pos])
      break;

    if (is_operator(val[pos])) {
      expand |= 1;
      break;
    }

    f = get_wf(val, &pos);
    if (!f)
      break;

    if (f && f->qs == QNONE && f->next == NULL) {
      if (expand & 1) {
        word = cat_wf(f);
        a = find_alias(word);
        if (a) {
          if (alias_depth >= MAX_ALIAS_DEPTH) {
            fprintf(stderr, "alias: too many levels of recursion\n");
            return;
          }
          alias_depth++;
          expand_alias(a->value, tokens, c);
          alias_depth--;
          expand &= ~1;
          continue;
        }
      }
      expand &= ~1;
    }

    if (f->word && f->word[0] != '\0') {
      tokens[*c].type = TWORD;
      tokens[*c].cmd = f;
      (*c)++;
      expand &= ~1;
    }
  }
}

