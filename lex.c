#include "simpsh.h"
#include "env.h"
#include "lex.h"
#include "utils.h"
#include "expand.h"

static wf **get_argv(const sh_tok *, size_t, size_t *);
static wf **get_assn(wf **, char ***);
static char *get_word(const char *line, size_t *p);
static void append_wf(wf **, wf **, char *, size_t, int);
static int is_assn(wf *);
static char * cat_wf(wf *wordf);

/* build consecutive TWORDS into command returned in word fragments */
static wf **
get_argv(const sh_tok *tokens, size_t cnt, size_t *i) {
  size_t t = *i;
  size_t j = 0;
  wf **argv = malloc(MAX_CMDS * sizeof(wf *));

  while (t < cnt && tokens[t].type == TWORD) {
    argv[j] = tokens[t].cmd;
    j++;
    t++;
  }

  argv[j] = NULL;
  *i = t;
  return argv;
} // get_argv

/* get word in char * line (not operators) */
static char *
get_word(const char *line, size_t *p) {
  size_t s = *p;
  size_t len = 0;

  s = skip_ws(line + s) - line; /* skip whitespace */

  if (!line[s])
    return NULL;

  while (line[s + len] &&
      line[s + len] != ' ' &&
      line[s + len] != '\t' &&
      line[s + len] != '\n' &&
      line[s + len] != '&' &&
      line[s + len] != '|' &&
      line[s + len] != ';' &&
      line[s + len] != '"' &&
      line[s + len] != '\'')
    len++;
  if (len == 0)
    return NULL;

  *p = s + len;
  return strndup(line + s, len);
} // get_word

/* check if word is name=value */
static int
is_assn(wf *cmd) {
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
} // is_assn

/* takes word fragment and returns (char *) */
static char *
cat_wf(wf *wordf) {
  size_t bufsize = 0;
  size_t buflen = 0, len;
  wf *f = wordf;

  for (; f; f = f->next)
    bufsize += strlen(f->word);
  char *buf = malloc(bufsize + 1);
  if (!buf)
    return NULL;
  buf[0] = '\0';

  f = wordf;
  for (; f; f = f->next) {
    len = strlen(f->word);
    bufcat(&buf, &bufsize, &buflen, f->word, len);
    if (!buf)
      return NULL;
  }
  return buf;
}

wf **
get_assn(wf **args, char ***sh_vars) {
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
  *sh_vars = malloc((a_c + 1) * sizeof(char *));
  if (!*sh_vars)
    return NULL;

  for (j = 0; j < a_c; j++)
    (*sh_vars)[j] = cat_wf(args[j]); /* NOTE: remember cat_wf returns null check that if bugs pop up from this */
  (*sh_vars)[a_c] = NULL;

  for (k = a_c; args[k]; k++)
    args[k - a_c] = args[k];
  args[k - a_c] = NULL;

  return args;
}

/* add word fragment onto the end of the linked list */
static void
append_wf(wf **head, wf **tail, char *buf, size_t len, int quoted) {
  wf *f = malloc(sizeof(wf));
  f->word = strndup(buf, len);
  f->qs = quoted;
  f->next = NULL;

  if (!*head)
    *head = f;
  else
    (*tail)->next = f;

  *tail = f;
}

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

wf *
get_wf(char *line, size_t *pos) {
  wf *head = NULL;
  wf *tail = NULL;
  wf *f = NULL;
  quoted state;
  size_t bufpos = 0, i = *pos;
  size_t bufsize = BUF_S;
  char c, n;
  char *buf = malloc(bufsize);
  if (!buf)
    return NULL;

  state = QNONE;

  while (line[i]) {
    c = line[i];
    n = line[i + 1];
    if (bufpos + 1 >= bufsize) {
      buf = s_realloc(buf, &bufsize);
      if (!buf) {
        f = head;
        freewf(f);
        return NULL;
      }
    }
    switch (state) {
    case QNONE:
      if (c == ' ' || c == '\t' || c == '\n') {
        goto done;
      } else if (c == '&' || c == '|' || c == ';') {
        goto done;
      } else if (c == '\'') {
        if (bufpos > 0) /* save unquoted frag if nonempty */
          append_wf(&head, &tail, buf, bufpos, state);
        bufpos = 0;
        state = QSINGLE;
        i++;
      } else if (c == '"') {
        if (bufpos > 0) /* save unquoted frag if nonempty */
          append_wf(&head, &tail, buf, bufpos, state);
        bufpos = 0;
        state = QDOUBLE;
        i++;
      } else if (c == '\\') {
        if (n == '\0') {
          i++;
          goto done;
        } else {
          buf[bufpos++] = n;
          i += 2;
        }
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;
    case QSINGLE:
      if (c == '\'') {
        if (bufpos > 0) /* save single quoted frag if nonempty */
          append_wf(&head, &tail, buf, bufpos, state);
        bufpos = 0;
        state = QNONE;
        i++;
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;
    case QDOUBLE:
      if (c == '"') {
        if (bufpos > 0) /* save double quoted frag if nonempty */
          append_wf(&head, &tail, buf, bufpos, state);
        bufpos = 0;
        state = QNONE;
        i++;
      } else if (c == '\\') {
        if (n == '\n') {
          i += 2;
          continue;
        } else if (n == '$' || n == '"' || n == '\\') {
          buf[bufpos++] = n;
          i += 2;
        } else {
          buf[bufpos++] = c;
          i++;
        }
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;
    }
  }

  done:
    append_wf(&head, &tail, buf, bufpos, state); /* save double quoted frag */
    free(buf);
    *pos = i;
    return head;
}

sh_tok *
tokenize(char *line, int *cnt) {
  size_t p = 0, c = 0, n;
  *cnt = 0;
  wf *f;

  if (!line) {
    *cnt = -1;
    return NULL;
  }
  if (strlen(line) == 0)
    return NULL;

  sh_tok *tokens = malloc(MAX_CMDS * (sizeof(sh_tok)));
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
      if (f->word && f->word[0] != '\0') {
        tokens[c].type = TWORD;
        tokens[c].cmd = f;
        c++;
      } else {
        freewf(f);
      }
    }
  }
  tokens[c].type = TEOF;
  tokens[c].cmd = NULL;

  *cnt = c;
  return tokens;
}

cmd_tree *
build_tree(const sh_tok *tokens, size_t cnt) {
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
    free_argv(args);
    return NULL;
  }
  args = get_assn(args, &sh_vars);
  /* if (sh_vars && (!args || !args[0])) {
  } */

  r = newcmdnode(args, negate, sh_vars);
  negate = FALSE;

  while (i < cnt && tokens[i].type != TEOF) {
    t = tokens[i].type;
    if (t != TAND && t != TOR && t != TSEMI)
      goto cleanup;
    i++; /* move to next command */

    if (i >= cnt || tokens[i].type != TWORD)
      goto cleanup;

    for (; i < cnt && tokens[i].type == TNOT; i++)
      neg++;
    negate = (neg % 2) ? TRUE : FALSE;
    neg = 0;

    args = get_argv(tokens, cnt, &i); /* Get next command's arguments */
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
  freectree(r);
  return NULL;
} /* build command abstract syntax tree */

char *
expand_alias(char *line) {
  char *eline = strdup(line);
  char *word, *nl;
  size_t s, p, n;
  int depth, q;
  alias *a;

  p = 0;
  depth = 0;

  while (eline[p]) {
    q = 0;
    p = skip_ws(line + p) - line; /* skip whitespace */
    if (!eline[p])
      break;

    n = p + 1;
    if ((eline[p] == '&' && eline[n] == '&') ||
        (eline[p] == '|' && eline[n] == '|') ||
        eline[p] == ';') {
      p += (eline[p] == eline[n]) ? 2 : 1;
      continue;
    }

    s = p;
    if (!(word = get_word(eline, &p)))
      break;
    if (q) {
      free(word);
      // continue;
    } else {
      a = find_alias(word);
      if (word)
        free(word);
      if (a) {
        if (depth < MAX_ALIAS_DEPTH) {
          nl = malloc(strlen(eline) + strlen(a->value) + 1);
          if (!nl) {
            free(eline);
            return line;
          }
          strncpy(nl, eline, s);
          nl[s] = '\0';
          strcat(nl, a->value);
          strcat(nl, eline + p);
          free(eline);
          eline = nl;
          p = 0;  // p = s + strlen(a->value);
          depth++;
          continue;
        } else {
          fprintf(stderr, "alias: too many levels of recursion");
          free(eline);
          return line;
        }
      }
    }
    while (eline[p]) {
      while (eline[p] == ' ' || eline[p] == '\n' || eline[p] == '\t')
        p++;
      if (!eline[p])
        break;

      n = p + 1;
      if ((eline[p] == '&' && eline[n] == '&') ||
          (eline[p] == '|' && eline[n] == '|') ||
          eline[p] == ';') {
        break;
        // p += (eline[p] == eline[n]) ? 2 : 1;
      }
      if (!(word = get_word(eline, &p)))
        break;
      free(word);
    }
  }
  if (line)
    free(line);
  return eline;
}
