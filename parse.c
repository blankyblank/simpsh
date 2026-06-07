/* parse.c - parser functions */
#include "exec.h"
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <string.h>

#include "malloc.h"
#include "parse.h"
#include "input.h"
#include "lex.h"
#include "opts.h"
#include "utils.h"

static const char tokendlist[] = {
  [TEOF]   = 1,
  [TRP]    = 1,
  [TRB]    = 1,
  [TTHEN]  = 1,
  [TELIF]  = 1,
  [TELSE]  = 1,
  [TFI]    = 1,
  [TDO]    = 1,
  [TDONE]  = 1,
};

static redir *heredoc_head;
static redir **heredoc_tail = &heredoc_head;

static void parse_heredoc(void);
static int is_assn(wf *);
static int get_assn(wf **, wf *** restrict);
static cmd_tree *parse_group(void);
static cmd_tree *parse_func(void);
static cmd_tree *parse_cmd(void);
static cmd_tree *parse_while(token);
static cmd_tree *parse_if(void);
static cmd_tree *parse_simple_cmd(size_t);

static inline cmd_tree *
newcmdnode(wf **args, int flags, wf **sh_vars, size_t vc)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = CMD;
  CARGS(n) = args;
  CVARS(n) = sh_vars;
  CVARC(n) = vc;
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

static inline cmd_tree *
newwhilenode(cmd_tree *cond, cmd_tree *body, token untilt)
{
  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = WHILE;
  n->left = cond;
  n->right = body;
  n->flags = (untilt == TUNTIL) ? UNTIL : 0;
  return n;
}

static inline cmd_tree *
newifnode(cmd_tree *cond, cmd_tree *then, cmd_tree *else_)
{
  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = IF;
  n->left = cond;
  n->right = then;
  CELSE(n) = else_;
  n->flags = 0;
  return n;
}

/* check if word is name=value */
static int
is_assn(wf *cmd)
{
  char *eq = memchr(cmd->word, '=', cmd->len);
  const char *p;

  if (cmd->qs != QNONE)
    return 0;
  if (!eq || eq == cmd->word)
    return 0;

  for (p = cmd->word; p < eq; p++) {
    if (p == cmd->word) {
      if (!isalpha_(*p) && *p != '_')
        return 0;
    } else {
      if (!isalnum_(*p) && *p != '_')
        return 0;
    }
  }
  return 1;
}

/** get name and value from NAME=value pair */
static int
get_assn(wf **args, wf ***restrict sh_vars)
{
  int i, j, k, ac;
  *sh_vars = NULL;

  if (!args)
    return 0;

  for (i = 0; args[i]; i++)
    if (!is_assn(args[i]))
      break;

  ac = i;
  if (!ac) {
    *sh_vars = NULL;
    return ac;
  }
  *sh_vars = st_alloc((ac + 1) * sizeof(wf *));
  if (!*sh_vars)
    return 0;

  for (j = 0; j < ac; j++)
    (*sh_vars)[j] = args[j];
  (*sh_vars)[ac] = NULL;

  for (k = ac; args[k]; k++)
    args[k - ac] = args[k];
  args[k - ac] = NULL;

  return ac;
}

__attribute__((hot)) cmd_tree *
parse_list(token s)
{
  sh_tok t;
  cmd_tree *l, *r;

  chkwd |= CHKALIAS | CHKKWD | (s != TEOF ? CHKNL : 0);
  t = tokenize();
  if ((s != TEOF && tokendlist[t.type]) || t.type == TEOF || t.type == TNL) {
    if (t.type != TEOF && t.type != s)
      last_tok = t;
    return NULL;
  }
  last_tok = t;

  l = parse_cmd();
  if (!l)
    return NULL;

  for (;;) {
    chkwd |= CHKALIAS | CHKKWD;
    t = tokenize();
    if (t.type == TSEMI || (t.type == TNL && s != TEOF) || t.type == TBKGRND) {
      chkwd |= CHKALIAS | CHKKWD;
      if (heredoc_head)
        parse_heredoc();
      sh_tok t2;
      t2 = tokenize();
      if (tokendlist[t2.type]) {
        last_tok = t2;
        break;
      }
      last_tok = t2;
      r = parse_cmd();
      if (!r) {
        if (t.type == TBKGRND)
          l = newoppnode(TBKGRND, l, NULL);
        break;
      }
      l = newoppnode(t.type, l, r);
    } else {
      if (heredoc_head)
        parse_heredoc();
      if (t.type == TEOF || t.type == s || tokendlist[t.type]) {
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
    chkwd |= CHKALIAS | CHKKWD;
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
    chkwd |= CHKALIAS | CHKKWD;
    return sub;
  }
  // Anything else is a syntax error
  last_tok = t;
  return NULL;
}

/*
 * parser for heredocs
 */
static void
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

cmd_tree *
parse_while(token tok)
{
  cmd_tree *condition, *body, *n;
  sh_tok t;

  if (!(condition = parse_list(TDO)))
    return NULL;
  t = tokenize();
  if (t.type != TDO)
    return NULL;
  if (!(body = parse_list(TDONE)))
    return NULL;
  t = tokenize();
  if (t.type != TDONE)
    return NULL;

  n = newwhilenode(condition, body, tok);
  chkwd |= CHKALIAS | CHKKWD;
  return n;
}

static cmd_tree *
parse_if(void)
{
  cmd_tree *cond, *then, *else_;
  sh_tok t;

  cond = parse_list(TTHEN);
  t = tokenize();
  if (t.type != TTHEN)
    return NULL;
  then = parse_list(TFI);
  t = tokenize();

  switch (t.type) {
    case TELIF:
      else_ = parse_if();
      break;
    case TELSE:
      else_ = parse_list(TFI);
      t = tokenize();
      if (t.type != TFI)
        return NULL;
      break;
    case TFI:
      else_ = NULL;
      break;
    default:
      return NULL;
  }
  return newifnode(cond, then, else_);
}

cmd_tree *
parse_simple_cmd(size_t neg)
{
  sh_tok t, close;
  wf **args, **sh_vars;
  redir *redirs, **tail;
  size_t vc, wc, cap;
  cmd_tree *body, *l;


  args = st_alloc(8 * sizeof(wf *));
  redirs = NULL;
  tail = &redirs;
  wc = 0;
  cap = 8;

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
          if (n.type == TREDIR && (t.cmd->flags & WFALLNUM)) {
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
    l = newcmdnode(NULL, (neg & 1) ? NEG : 0, NULL, 0);
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

  vc = get_assn(args, &sh_vars);
  l = newcmdnode(args, (neg & 1) ? NEG : 0, sh_vars, vc);
  if (redirs)
    return newredirnode(l, redirs);
  return l;

}


/**  recursive decent parser  */
__attribute__((hot)) cmd_tree *
parse_cmd(void)
{
  size_t neg;
  cmd_tree *sub, *l, *r, *r2;
  sh_tok t, t2;

  l = NULL;
  neg = 0;

  for (;;) {
    t = tokenize();
    if (t.type != TNOT) {
      last_tok = t;
      break;
    }
    chkwd |= CHKALIAS | CHKKWD;
    neg++;
  }

  t = tokenize();

  switch (t.type) {
    case TIF:
      return parse_if();
    case TWHILE:
    case TUNTIL:
      return parse_while(t.type);
    case TLP:
      sub = parse_list(TRP);
      t = tokenize();
      if (t.type != TRP)
        return NULL;
      chkwd |= CHKALIAS | CHKKWD;
      return newsubsh(sub, (neg & 1) ? NEG : 0);
    case TTHEN:
    case TELIF:
    case TELSE:
    case TFI:
    case TDO:
    case TDONE: // TODO: figure out where to print error message
      t.type = TWORD;
      break;
    default:
      // TODO: add  for loops
      break;
  }

  last_tok = t;

  if (!(l = parse_simple_cmd(neg)))
    return NULL;

  // parse_pipe
  for (;;) {
    t = tokenize();
    if (t.type == TPIPE) {
      chkwd |= CHKALIAS | CHKNL | CHKKWD;
      r = parse_simple_cmd(0);
      if (!r)
        return NULL;
      l = newoppnode(t.type, l, r);
    } else {
      last_tok = t;
      break;
    }
  }

  for (;;) {
    chkwd |= CHKALIAS | CHKKWD;
    t = tokenize();
    if (t.type == TAND || t.type == TOR) {
      chkwd |= CHKALIAS | CHKNL | CHKKWD;
      if (!(r = parse_simple_cmd(0)))
        return NULL;
      for (;;) {
        t2 = tokenize();
        if (t2.type == TPIPE) {
          chkwd |= CHKALIAS | CHKNL | CHKKWD;
          if (!(r2 = parse_simple_cmd(0)))
            return NULL;
          r = newoppnode(TPIPE, r, r2);
        } else {
          last_tok = t2;
          break;
        }
      }
      if (eflag) {
        l->flags |= EFLAG_SAFE;
        if (l->right)
          l->right->flags |= EFLAG_SAFE;
      }
      l = newoppnode(t.type, l, r);
    } else {
      last_tok = t;
      break;
    }
  }

    if (l->type == CMD) {
      CSTATUS(l) = run_commands(l);
      l->flags |= EXECED;
    }
  return l;
}
