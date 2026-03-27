#include "simpsh.h"
#include "alias.h"
#include "builtins.h"
#include "lex.h"

typedef enum {
  QNONE,
  QSINGLE,
  QDOUBLE,
} quoted;

static char **get_argv(const sh_tok *, size_t, size_t *);

static char **
get_argv(const sh_tok *tokens, size_t cnt, size_t *i) {
  size_t t = *i;
  size_t j = 0;
  char **argv = malloc(MAX_CMDS * sizeof(char *));

  while (t < cnt && tokens[t].type == TWORD) {
    argv[j] = strdup(tokens[t].cmd);
    if (!argv[j]) {
      freeptr(argv);
      return NULL;
    }
    j++;
    t++;
  }

  argv[j] = NULL;
  *i = t;
  return argv;
} /* build consecutive TWORDS into command */


/*
 * shell language tokenizer. parses a line extracts a word, handles quoting,
 * variable expansion, escaping.
 *
 * it's used in tokenize, it returns immediately if it's at the position of an
 * opterator tokenize handles finding those. also whitespace etc.
 *
 * it advances the position for the caller, then the caller needs to advance
 * through whitespace, and operators when it handles them.
 */

// TODO: eventually try moving to a stack buffer or some other method to use stack memory
char *
get_word(char *line, size_t *pos, int *q) {
  quoted state;
  size_t bufpos = 0, i = *pos;
  size_t bufsize = BUF_S;
  size_t var_l;
  char *var;
  char c, n;
  *q = 0;

  // caller frees
  char *buf = malloc(bufsize);
  if (!buf)
    return NULL;

  state = QNONE;

  while (line[i]) {
    c = line[i];
    n = line[i + 1];
    if (bufpos + 1 >= bufsize) {
      buf = s_realloc(buf, &bufsize);
      if (!buf)
        return NULL;
    }

    switch (state) {
    case QNONE:
      if (c == ' ' || c == '\t' || c == '\n') {
        goto done;
      } else if (c == '&' || c == '|' || c == ';') {
        goto done;
      } else if (c == '\'') {
        state = QSINGLE;
        *q = 1;
        i++;
      } else if (c == '"') {
        state = QDOUBLE;
        *q = 1;
        i++;
      } else if (c == '\\') {
        if (n == '\0') {
          i++;
          goto done;
        } else {
          buf[bufpos++] = n;
          i += 2;
        }
      } else if (c == '$') {
        var = exp_var(line, &i, &var_l); // NOTE: exp_var updates i
        if (!var)
          return NULL;
        if (bufpos + var_l >= bufsize) {
          buf = s_realloc(buf, &bufsize);
          if (!buf) {
            perror("realloc failed");
            return NULL;
          }
        }
        memcpy(buf + bufpos, var, var_l);
        free(var);
        bufpos += var_l;
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;

    case QSINGLE:
      if (c == '\'') {
        state = QNONE;
        i++;
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;

    case QDOUBLE:
      if (c == '"') {
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
      } else if (c == '$') {
        var = exp_var(line, &i, &var_l); // NOTE: exp_var updates i
        if (!var)
          return NULL;
        if (bufpos + var_l >= bufsize) {
          buf = s_realloc(buf, &bufsize);
          if (!buf) {
            perror("realloc failed");
            return NULL;
          }
        }
        memcpy(buf + bufpos, var, var_l);
        free(var);
        bufpos += var_l;
      } else {
        buf[bufpos++] = c;
        i++;
      }
      break;
    }
  }

done:
  buf[bufpos] = '\0';
  *pos = i;
  return buf;
}

sh_tok *
tokenize(char *line, int *cnt) {
  size_t p = 0, c = 0, n;
  int q;
  *cnt = 0;
  char *buf = NULL;

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
    while (line[p] == ' ' || line[p] == '\n' || line[p] == '\t') /* skip whitespace */
      p++;
    if (!line[p])
      break;

    n = p + 1;
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
      if (!(buf = get_word(line, &p, &q)))
        return NULL;
      if (buf[0] != '\0') {
        tokens[c].type = TWORD;
        tokens[c].cmd = buf;
        c++;
      } else
        free(buf);
    }
  }
  tokens[c].type = TEOF;
  tokens[c].cmd = NULL;

  *cnt = c;
  return tokens;
}

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
    /* skip whitespace */
    while (eline[p] == ' ' || eline[p] == '\n' || eline[p] == '\t')
      p++;
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
    if (!(word = get_word(eline, &p, &q)))
      break;
    if (q) {
      free(word);
      // continue;
    } else {
      a = lookup_alias(word);
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
          p = 0; // p = s + strlen(a->value);
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
        // p += (eline[p] == eline[n]) ? 2 : 1;
        break;
      }
      if (!(word = get_word(eline, &p, &q)))
        break;
      free(word);
    }
  }
  if (line)
    free(line);
  return eline;
}

// TODO: add in negation parsing logic, also run_commands needs ot handle it.
cmd_tree *
build_tree(const sh_tok *tokens, size_t cnt) {
  size_t i = 0;
  char **args = NULL;
  cmd_tree *n, *r;
  cmd_false negate = FALSE, neg = 0;
  token t;

  if (cnt < 1 || tokens[i].type != TWORD)
    return NULL;

  while (tokens[i].type == TNOT) {
    i++;
    neg++;
  }
  negate = (neg % 2) ? TRUE : FALSE;

  args = get_argv(tokens, cnt, &i);

  if (!args || !args[0]) { /* handle empty input */
    fprintf(stderr, "somethings wrong \n");
    freeptr(args);
    return NULL;
  }

  r = newcmdnode(args, negate);
  negate = 0;

  while (i < cnt && tokens[i].type != TEOF) {
    t = tokens[i].type;

    if (t != TAND && t != TOR && t != TSEMI) {
      // FIXME: I'm leaking memory here I'm pretty sure
      fprintf(stderr, "Expected Operator\n");
      return NULL;
    }
    i++;  /* move to next command */

    if (i >= cnt || tokens[i].type != TWORD) /* Must have a command after operator */
      return NULL;

    while (tokens[i].type == TNOT) {
      i++;
      neg++;
    }
    negate = (neg % 2) ? TRUE : FALSE;

    args = get_argv(tokens, cnt, &i); /* Get next command's arguments */
    if (!args || !args[0]) {
      freeptr(args);
      return NULL;
    }

    n = newcmdnode(args, 0);
    negate = FALSE;
    r = newoppnode(t, r, n);
  }
  return r;
} /* build command abstract syntax tree */

int
run_commands(const cmd_tree *n) {
  int l_status, r_status;

  if (!n)
    return 0;

  if (n->type == CMD) {
    if (getbuiltin(n->args) == 1)
      return builtin_launch(n->args);
    else
      return shexec(n->args);
  }

  if (n->type == OP) {
    l_status = run_commands(n->left);

    switch (n->op_t) {
    case TSEMI:
      r_status = run_commands(n->right);
      return r_status;
    case TAND:
      if (l_status != 0)
        return l_status;
      r_status = run_commands(n->right);
      return r_status;
    case TOR:
      if (l_status == 0)
        return l_status;
      r_status = run_commands(n->right);
      return r_status;
    case TEOF:
      return l_status;
    default:
      fprintf(stderr, "Unknown Operator\n");
      return 1;
    }
  }

  return 0;
} /* run the commands previously built into the tree  */
