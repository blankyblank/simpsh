#include "simpsh.h"
#include "parser.h"

char **get_argv(sh_tok *, size_t, size_t *);

sh_tok * // TODO: not sure I will even need these
copy_tokens(sh_tok *src, int start, int len) {
  sh_tok *dst = malloc((len + 1) * sizeof(sh_tok));
  if (!dst)
    return NULL;
  for (int i = 0; i < len; i++) {
    dst[i] = src[start + i];
    if (src[start + i].cmd)
      dst[i].cmd = strdup(src[start + i].cmd);
  }
  dst[len].type = TEOF;
  dst[len].cmd = NULL;
  return dst;
}

sh_tok * // TODO: not sure I will even need these
concat_tokens(sh_tok *a, int cnt_a, sh_tok *b, int cnt_b) {
  sh_tok *c = malloc((cnt_a + cnt_b + 1) * sizeof(sh_tok));
  if (!c)
    return NULL;
  for (int i = 0; i < cnt_a; i++)
    c[i] = a[i];
  for (int i = 0; i < cnt_b; i++) {
    c[cnt_a + i] = b[i];
    if (b[i].cmd)
      c[cnt_a + i].cmd = strdup(b[i].cmd);
  }
  c[cnt_a + cnt_b].type = TEOF;
  c[cnt_a + cnt_b].cmd = NULL;
  return c;
}

char **
get_argv(sh_tok *tokens, size_t cnt, size_t *i) {
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
      continue;
    }

    a = lookup_alias(word);
    if (word)
      free(word);
    if (a) {
      if (depth < MAX_ALIAS_DEPTH) {
        nl = malloc(strlen(eline) + strlen(a->value) + 1);
        if (!nl) {
          free(eline);
          return line; // not sure this is the way to handle this. maybe return null instead.
        }
        strncpy(nl, eline, s);
        nl[s] = '\0';
        strcat(nl, a->value);
        strcat(nl, eline + p);
        free(eline);
        eline = nl;
        p = s + strlen(a->value);
        depth++;
      } else {
        fprintf(stderr, "alias: too many levels of recursion");
        free(eline);
        return line;
      }
    }
  }
  if (line)
    free(line);
  return eline;
}

cmd_tree *
build_tree(sh_tok *tokens, size_t cnt) {
  size_t i = 0;
  char **args = NULL;
  cmd_tree *n, *r;
  token t;

  if (cnt < 1 || tokens[i].type != TWORD)
    return NULL;

  args = get_argv(tokens, cnt, &i);

  if (!args || !args[0]) { /* handle empty input */
    fprintf(stderr, "somethings wrong \n");
    freeptr(args);
    return NULL;
  }

  r = newcmdnode(args, 0);

  while (i < cnt && tokens[i].type != TEOF) {
    t = tokens[i].type;

    if (t != TAND && t != TOR && t != TSEMI) {
      fprintf(stderr, "Expected Operator\n");
      return NULL;
    }
    i++;  /* move to next command */ 

    // Must have a command after operator
    if (i >= cnt || tokens[i].type != TWORD)
      return NULL;
    /* Get next command's arguments */
    args = get_argv(tokens, cnt, &i);
    if (!args || !args[0]) {
      freeptr(args);
      return NULL;
    }

    n = newcmdnode(args, 0);
    r = newoppnode(t, r, n);
  }
  return r;
} /* build command tree recursively */

int
run_commands(cmd_tree *n) {
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
