/* lex.c - tokenizer and parser functions */
#include "simpsh.h"
#include "env.h"
#include "lex.h"
#include "utils.h"
#include "malloc.h"

static wf **get_argv(const sh_tok *, size_t, size_t *);
static wf **get_assn(wf **, char ***);
static void append_wf(wf **, wf **, char *, int);
static int is_assn(wf *);
static char *cat_wf(wf *wordf);
static void expand_alias(char *val, sh_tok *tokens, size_t *c);

static int alias_depth = 0;

static inline char *
cmd_end(char *s)
{
  while (*s && !(*s == ' ' || *s == '\t' || *s == '\n' || *s == '&' ||
                 *s == '|' || *s == ';' || *s == '"' || *s == '\''))
    s++;
  return s;
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

  for (j = 0; j < a_c; j++) {
    /*  NOTE: remember cat_wf returns null check that if bugs pop up from this
     */
    (*sh_vars)[j] = cat_wf(args[j]);
  }
  (*sh_vars)[a_c] = NULL;

  for (k = a_c; args[k]; k++)
    args[k - a_c] = args[k];
  args[k - a_c] = NULL;

  return args;
}

/**
 * add word fragment onto the end of the linked list
 */
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
      if (c == ' ' || c == '\t' || c == '\n') {
        goto done;
      } else if (c == '&' || c == '|' || c == ';') {
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

sh_tok *
tokenize(char *line, int *cnt)
{
  size_t p = 0, c = 0, n;
  *cnt = 0;
  wf *f = NULL;
  char *word;
  alias *a;

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
      continue;
    } else if (line[p] == '|' && line[n] == '|') {
      tokens[c].type = TOR;
      tokens[c].cmd = NULL;
      c++;
      p += 2;
      continue;
    } else if (line[p] == ';') {
      tokens[c].type = TSEMI;
      tokens[c].cmd = NULL;
      c++;
      p++;
      continue;
    } else {
      if (!(f = get_wf(line, &p)))
        return NULL;
      if (f && f->qs == QNONE && f->next == NULL) {
        word = cat_wf(f);
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
      }
    }
  }
  tokens[c].type = TEOF;
  tokens[c].cmd = NULL;

  *cnt = c;
  return tokens;
}

/**  build command abstract syntax tree  */
cmd_tree *
build_tree(const sh_tok *tokens, size_t cnt)
{
  size_t i = 0;
  wf **args = NULL;
  cmd_tree *nxt, *r;
  cmd_false negate = FALSE, neg = 0;
  char **sh_vars = NULL;
  token t;

  for (; i < cnt && tokens[i].type == TNOT; i++)
    neg++;
  negate = (neg % 2) ? TRUE : FALSE;
  neg = 0;

  if (i >= cnt || tokens[i].type != TWORD) /* check for command */
    return NULL;
  args = get_argv(tokens, cnt, &i);
  if (!args || !args[0]) { /* handle empty input */
    fprintf(stderr, "TODO: double check this at some point \n");
    return NULL;
  }
  args = get_assn(args, &sh_vars);

  r = newcmdnode(args, negate, sh_vars);
  negate = FALSE;

  while (i < cnt && tokens[i].type != TEOF) {
    t = tokens[i].type;
    if (t != TAND && t != TOR && t != TSEMI)
      goto cleanup;
    /* move to next command */
    i++;
    if (i >= cnt || tokens[i].type != TWORD)
      goto cleanup;
    for (; i < cnt && tokens[i].type == TNOT; i++)
      neg++;
    negate = (neg % 2) ? TRUE : FALSE;
    neg = 0;
    /* Get next command's arguments */
    args = get_argv(tokens, cnt, &i);
    if (!args || !args[0])
      goto cleanup;
    args = get_assn(args, &sh_vars);
    nxt = newcmdnode(args, negate, sh_vars);
    negate = FALSE;
    r = newoppnode(t, r, nxt);
  }
  return r;

cleanup:
  fprintf(stderr, "Syntax error\n");
  return NULL;
}

static void
expand_alias(char *val, sh_tok *tokens, size_t *c)
{
  wf *f;
  alias *a;
  char *word;
  size_t pos = 0;

  while (val[pos]) {
    pos = skip_ws(val + pos) - val;
    if (!val[pos])
      break;

    if (val[pos] == '&' || val[pos] == '|' || val[pos] == ';')
      break;

    f = get_wf(val, &pos);
    if (!f)
      break;

    if (f && f->qs == QNONE && f->next == NULL) {
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
        continue;
      }
    }

    if (f->word && f->word[0] != '\0') {
      tokens[*c].type = TWORD;
      tokens[*c].cmd = f;
      (*c)++;
    }
  }
}

