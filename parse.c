/* parse.c - parser functions */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <string.h>

#include "alloc.h"
#include "error.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "opts.h"
#include "parse.h"
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

static const char *
tokstr(token t)
{
  static const char *toks[] = {
    [TWORD] = "word",   [TEOF] = "end of file",
    [TIF] = "if",       [TTHEN] = "then",
    [TELIF] = "elif",   [TELSE] = "else",
    [TFI] = "fi",       [TCASE] = "case",
    [TESAC] = "esac",   [TWHILE] = "while",
    [TUNTIL] = "until", [TFOR] = "for",
    [TIN] = "in",       [TDO] = "do",
    [TDONE] = "done",   [TNOT] = "!",
    [TPIPE] = "|",      [TAND] = "&&",
    [TOR] = "||",       [TSEMI] = ";",
    [TNL] = "newline",  [TLP] = "(",
    [TRP] = ")",        [TLB] = "{",
    [TRB] = "}",        [TBKGRND] = "&",
    [TDSEMI] = ";;",    [TREDIR] = "redirection",
    [TCMDSUB] = "$(",
  };
  return (t >= 0 && (size_t)t < arsz(toks, toks[0]) && toks[t]) ? toks[t] : "unknown";
}

/* syntax warning error */
void *
syntxerr(char *msg, token t)
{
  fprintf(stderr, "%s: syntax error: %s: \"%s\"\n", shname, msg, tokstr(t));
  lstatus = 2;
  return NULL;
}

/* syntax warning error with line number */
void *
lnsyntxerr(int ln, char *msg, token t)
{
  char lnbuf[256];
  size_t l;

  l = lltoa(ln, lnbuf);
  lnbuf[l] = '\0';
  fprintf(stderr, "%s: %s: syntax error: %s: \"%s\"\n", shname, lnbuf, msg, tokstr(t));
  lstatus = 2;
  return NULL;
}

static redir *heredoc_head;
static redir **heredoc_tail = &heredoc_head;
#define WFCAP 8

#define gettok(f, t) (chkwd |= (f), (t) = tokenize())
static void parse_heredoc(void);
static int is_assn(wf *);
static int get_assn(wf **, wf *** restrict);
static cmd_tree *parse_group(void);
static cmd_tree *parse_func(void);
static cmd_tree *parse_cmd(void);
static cmd_tree *parse_for(void);
static cmd_tree *parse_while(token);
static cmd_tree *parse_if(void);
static cmd_tree *parse_case(void);
static cmd_tree *parse_simple_cmd(size_t);

static inline cmd_tree *
newcmdnode(wf **restrict args, int flags, wf **restrict sh_vars, size_t vc)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = CMD;
  CARGS(n) = args;
  CVARS(n) = sh_vars;
  CVARC(n) = vc;
  n->flags = flags;
  n->line = shinpt->linenum;
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
  n->line = shinpt->linenum;
  return n;
}

static inline cmd_tree *
newfuncnode(wf *restrict name, cmd_tree *restrict body)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = FUNC;
  CFUNC(n) = name;
  n->left = body;
  n->line = shinpt->linenum;
  /* redir everything else is empty */
  return n;
}

static inline cmd_tree *
newredirnode(cmd_tree *restrict l, redir *restrict r)
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
  n->line = shinpt->linenum;
  return n;
}

static inline cmd_tree *
newoppnode(token opp_t, cmd_tree *l, cmd_tree *r)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = OP;
  COPP(n) = opp_t;
  n->left = l;
  n->right = r;
  n->flags = 0;
  n->line = shinpt->linenum;
  return n;
}

static inline cmd_tree *
newfornode(wf *name, wf **words, cmd_tree *body)
{
  cmd_tree *n;
    n = st_alloc(sizeof(cmd_tree));
  n->type = FOR;
  CFOR(n).name = name;
  CFOR(n).words = words;
  n->right = body;
  n->flags = 0;
  n->line = shinpt->linenum;
  return n;
}

static inline cmd_tree *
newwhilenode(cmd_tree *restrict cond, cmd_tree *restrict body, token untilt)
{
  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = WHILE;
  n->left = cond;
  n->right = body;
  n->flags = (untilt == TUNTIL) ? UNTIL : 0;
  n->line = shinpt->linenum;
  return n;
}
static inline cmd_tree *
newcasenode(wf *word, clause *clauses)
{
  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = CASE;
  CCASE(n).word = word;
  CCASE(n).clauses = clauses;
  n->flags = 0;
  n->line = shinpt->linenum;
  return n;
}

static inline cmd_tree *
newifnode(cmd_tree *restrict cond, cmd_tree *restrict then, cmd_tree *restrict else_)
{
  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = IF;
  n->left = cond;
  n->right = then;
  CELSE(n) = else_;
  n->flags = 0;
  n->line = shinpt->linenum;
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

  gettok(CHKALIAS | CHKKWD | (s != TEOF ? CHKNL : 0), t);
  if ((s != TEOF && tokendlist[t.type]) || t.type == TEOF || t.type == TNL) {
    if (t.type != TEOF && t.type != s)
      last_tok = t;
    return NULL;
  }
  last_tok = t;

  l = parse_cmd();
  if (!l)
    return NULL;

  if (heredoc_head)
    parse_heredoc();

  for (;;) {
    // if (iflag && !shinput_avail())
    //     break;
    gettok(CHKALIAS | CHKKWD, t);
    if (t.type == TSEMI || (t.type == TNL && s != TEOF) || t.type == TBKGRND) {
      sh_tok t2;
      gettok(CHKALIAS | CHKKWD, t2);
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
    return parserr(curline, "expected", TRB);
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
      return parserr(curline, "expected", TRP);
    chkwd |= CHKALIAS | CHKKWD;
    return sub;
  }
  // Anything else is a syntax error
  last_tok = t;
  return parserr(curline, "unexpected token", t.type);
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
          llen = stnext - lpos;
          if (llen == eofvlen && memcmp(lpos, eofv, eofvlen) == 0) {
            stunalloc(lpos);
            goto done;
          }
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

done:
    bodylen = stnext - bpos;
    r->heredoc = grab_str(bodylen);
  }
  heredoc_tail = &heredoc_head;
}

static cmd_tree*
parse_case(void)
{
  clause *clauses, *headcl, *tailcl;
  wf *word;
  sh_tok t;
  size_t cap, pc;

  tailcl = NULL;
  headcl = NULL;
  clauses = NULL;
  gettok(CHKALIAS | CHKKWD, t);
  if (t.type != TWORD)
    return parserr(curline, "unexpected token", t.type);
  word = t.cmd;

  gettok(CHKALIAS | CHKKWD, t);
  if (t.type == TIN) {
    cap = WFCAP;
    for (;;) {
      chkwd = CHKNL | CHKKWD;
      t = tokenize();
      if (t.type == TESAC)
        break;
      if (t.type == TLP)
        t = tokenize();

      clauses = st_alloc(sizeof(clause));
      clauses->ptrn= st_alloc(cap * sizeof(wf *));
      clauses->next = NULL;
      clauses->body = NULL;
      pc = 0;
      while (t.type == TWORD || t.type == TPIPE) {
        if (pc >= cap) {
          cap *= 2;
          wf **new = st_alloc(cap * sizeof(wf *));
          memcpy(new, clauses->ptrn, pc * sizeof(wf *));
          clauses->ptrn = new;
        }
        if (t.type == TWORD)
          clauses->ptrn[pc++] = t.cmd;
        t = tokenize();
      }
      if (!pc)
        return NULL;
      clauses->ptrn[pc] = NULL;
      if (t.type != TRP)
        return parserr(curline, "expected", TRP);

      chkwd |= CHKALIAS | CHKKWD | CHKNL;
      clauses->body = parse_list(TDSEMI);
      if (!headcl)
        headcl = clauses;
      else
        tailcl->next = clauses;
      tailcl = clauses;
      t = tokenize();
      if (t.type == TDSEMI)
        continue;
      else if (t.type == TESAC)
        break;
      else
       return parserr(curline, "expected", TESAC);
    }
  } else {
    return parserr(curline, "expected", TIN);
  }
  chkwd |= CHKALIAS | CHKKWD;
  return newcasenode(word, headcl);
}


cmd_tree *
parse_for(void)
{
  cmd_tree  *body, *n;
  size_t wc, cap;
  wf *name, **words;
  sh_tok t;

  words = NULL;
  wc = 0;
  gettok(CHKALIAS | CHKKWD, t);
  if (t.type != TWORD)
    return parserr(curline, "expected", TWORD);
  name = t.cmd;

  gettok(CHKALIAS | CHKKWD, t);
  if (t.type == TIN) {
    cap = WFCAP;
    chkwd = (chkwd & ~CHKALIAS) | CHKNL;
    words = st_alloc(cap * sizeof(wf *));
    for (;;) {
      t = tokenize();
      if (t.type != TWORD)
        break;
      if (wc >= cap) {
        cap *= 2;
        wf **new = st_alloc(cap * sizeof(wf *));
        memcpy(new, words, wc * sizeof(wf *));
        words = new;
      }
      words[wc++] = t.cmd;
    }
    words[wc] = NULL;
    if (!wc)
      return parserr(curline, "expected list before", TSEMI);
  }

  chkwd |= CHKALIAS | CHKKWD;
  if (t.type == TSEMI) {
    t = tokenize();
    if (t.type != TDO)
      return parserr(curline, "expected", TDO);
  }

  if (!(body = parse_list(TDONE)))
    return NULL;
  gettok(CHKALIAS | CHKKWD, t);
  if (t.type != TDONE)
    return parserr(curline, "expected", TDONE);
  n = newfornode(name, words, body);
  chkwd |= CHKALIAS | CHKKWD;
  return n;
}

cmd_tree *
parse_while(token tok)
{
  cmd_tree *condition, *body;
  sh_tok t;

  if (!(condition = parse_list(TDO)))
    return NULL;
  t = tokenize();
  if (t.type != TDO)
    return parserr(curline, "expected", TDO);
  if (!(body = parse_list(TDONE)))
    return NULL;
  t = tokenize();
  if (t.type != TDONE)
    return parserr(curline, "expected", TDONE);

  chkwd |= CHKALIAS | CHKKWD;
  return newwhilenode(condition, body, tok);
}

static cmd_tree *
parse_if(void)
{
  cmd_tree *cond, *then, *else_;
  sh_tok t;

  cond = parse_list(TTHEN);
  t = tokenize();
  if (t.type != TTHEN)
    return parserr(curline, "expected", TTHEN);
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
        return parserr(curline, "expected", TFI);
      break;
    case TFI:
      else_ = NULL;
      break;
    default:
      return parserr(curline, "unexpected token", t.type);
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
  int cmdflags;

  sh_lineno = shinpt->linenum;
  // sh_lineno = curline;
  args = st_alloc(8 * sizeof(wf *));
  redirs = NULL;
  tail = &redirs;
  wc = 0;
  cap = WFCAP;
  cmdflags = (neg & 1) ? NEG : 0;

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
            return parserr(curline, "missing filename for", t.type);
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
          sh_tok n;
          int adj = 0;
          if (t.cmd->flags & WFALLNUM) {
            char c = shgetchar();
            if (c != SHEOF && (c == '<' || c == '>')) {
              adj = 1;
              if (c != SHEOF)
                shungetc(c);
            } else {
              if (c != SHEOF)
                shungetc(c);
            }
          }
          n = tokenize();
          if (n.type == TREDIR && (t.cmd->flags & WFALLNUM) && adj) {
            sh_tok name = tokenize();
            if (name.type != TWORD)
              return parserr(curline, "missing filename for", t.type);
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
          if (t.cmd->flags & WFCMDSUB)
            cmdflags |= NECMDSUB;
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
    l = newcmdnode(NULL, cmdflags, NULL, 0);
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
  l = newcmdnode(args, cmdflags, sh_vars, vc);
  if (redirs)
    return newredirnode(l, redirs);
  return l;

}

__attribute__((hot)) cmd_tree *
parse_cmd(void)
{
  size_t neg;
  cmd_tree *sub = NULL, *l, *r = NULL, *cmb = NULL;
  sh_tok t;
  int op;

  for (;;) {
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
        if (!(l = parse_if()))
          return NULL;
        break;
      case TWHILE:
      case TUNTIL:
        if (!(l = parse_while(t.type)))
          return NULL;
        break;
      case TFOR:
        if (!(l = parse_for()))
          return NULL;
        break;
      case TLP: /* subsh */
        sub = parse_list(TRP);
        t = tokenize();
        if (t.type != TRP)
          return parserr(curline, "expected", TRP);
        chkwd |= CHKALIAS | CHKKWD;
        l = newsubsh(sub, (neg & 1) ? NEG : 0);
        neg = 0;
        break;
      case TLB:
        l = st_alloc(sizeof(cmd_tree));
        l->type = BRACE;
        l->left = parse_group();
        if (!l->left)
          return NULL;
        l->flags = (neg & 1) ? NEG : 0;
        neg = 0;
        break;
      case TCASE:
        if (!(l = parse_case()))
          return NULL;
        break;
      case TTHEN:
      case TELIF:
      case TELSE:
      case TFI:
      case TDO:
      case TDONE:
        t.type = TWORD;
        break;
      default:
        break;
    }
    if (!l) {
      last_tok = t;
      if (!(l = parse_simple_cmd(0)))
        return NULL;
    }

    /* parse_pipe */
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
    if (neg & 1)
      l->flags |= NEG;

    if (!cmb)
      cmb = l;
    else {
      if (eflag) {
        cmb->flags |= EFLAG_SAFE;
        if (cmb->right)
          cmb->right->flags |= EFLAG_SAFE;
      }
      cmb = newoppnode(op, cmb, l);
    }

    chkwd |= CHKALIAS | CHKKWD;
    t = tokenize();
    if (t.type == TAND || t.type == TOR) {
      chkwd |= CHKALIAS | CHKNL | CHKKWD;
      op = t.type;
      continue;
    }
    last_tok = t;
    break;
  }
  return cmb;
}
