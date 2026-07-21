/* parse.c - parser functions */
/* NOLINT(build/c++11) */

#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "alloc.h"
#include "errmsg.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "opts.h"
#include "parse.h"
#include "utils.h"

#define WFCAP 8

static redir *heredoc_head;
static redir **heredoc_tail = &heredoc_head;
sh_tok tbuf = { .type = TNONE };
int tokflags;

#define gettok(f, t) (chkwd |= (f), (t) = tokenize())
#define peektok(t, f) ((tbuf.type == TNONE) ? (chkwd |= (f), tbuf = tokenize()) : tbuf, (t) = tbuf)
#define nexttok(t, f) ((tbuf.type != TNONE) ? ((t) = tbuf, tbuf = SHTOK(TNONE), (t)) : (chkwd |= (f), (t) = tokenize()))
#define tokreset() (tbuf = SHTOK(TNONE))
#define tokpushbk(t) (tbuf = (t))

static int is_assn(wf *);
static int get_assn(wf **, wf *** restrict);
static cmd_tree *parse_andor(void);
static cmd_tree * parse_subsh(void);
static cmd_tree *parse_simple_cmd(size_t);
static cmd_tree *parse_pipe(void);
static cmd_tree *parse_group(void);
static cmd_tree *parse_func(void);
static redir *parse_redir(sh_tok, int);
static void parse_heredoc(void);
static cmd_tree *parse_case(void);
static cmd_tree *parse_if(void);
static cmd_tree *parse_for(void);
static cmd_tree *parse_while(token);
static cmd_tree *parse_cmd(void);
static const char *tokstr(token t);
static char *errtok(sh_tok);
static void *synunexpected(int, sh_tok);
static void *synexpected(int, sh_tok, token);
static void *syntxerr(int, char *, token);

static inline cmd_tree *
newredirnode(cmd_tree * restrict l, redir * restrict r)
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
  n->line = curline;
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
  n->line = curline;
  return n;
}

static inline cmd_tree *
newcmdnode(wf ** restrict args, int flags, wf ** restrict sh_vars, size_t vc)
{
  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = CMD;
  CARGS(n) = args;
  CVARS(n) = sh_vars;
  CVARC(n) = vc;
  n->flags = flags;
  n->line = curline;
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
get_assn(wf **args, wf *** restrict sh_vars)
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

/* TODO:
 * fix cascading errors, to stop error message once one syntax error is found.
 */

__attribute__((hot)) cmd_tree *
parse_list(int multi)
{
  sh_tok t;
  cmd_tree *l = NULL;

  peektok(t, CHKALIAS |CHKKWD | (multi ? CHKNL : 0));
  if (t.type == TEOF || t.type == TNL)
    return NULL;

  for (;;) {
    cmd_tree *r;

    if (!(r = parse_andor())) {
      return l;
    }
    if (heredoc_head)
      parse_heredoc();
    
    peektok(t, CHKALIAS | CHKKWD);
    if (t.type == TBKGRND) {
      tokreset();
      l = newoppnode(TSEMI, l ? l : newoppnode(TBKGRND, r, NULL), l ? r : NULL);
      continue;
    }
    if (t.type != TSEMI && !(multi && t.type == TNL))
      return l = l ? newoppnode(TSEMI, l, r) : r;
    tokreset();
    l = l ? newoppnode(TSEMI, l, r) : r;
  }
}

cmd_tree *
parse_simple_cmd(size_t neg)
{
  sh_tok t;
  wf **args, **sh_vars;
  redir *redirs, **tail;
  cmd_tree *body, *l;

  size_t vc, wc, cap;
  int cmdflags;

  gstate.lineno = curline;
  cap = WFCAP;
  args = st_alloc(cap * sizeof(wf *));
  redirs = NULL;
  tail = &redirs;
  wc = 0;
  cmdflags = (neg & 1) ? NEG : 0;

  for (;;) {
    peektok(t, 0);
    switch (t.type) {
      case TREDIR:
        tokreset();
        redir *r;
        if (!(r = parse_redir(t, -1)))
          return syntxerr(curline, "missing filename for", t.type);
        *tail = r;
        tail = &r->next;
        continue;
      case TWORD:
        tokreset();
        {
          sh_tok n;
          peektok(n, 0);
          // assert(t.cmd);
          if (n.type == TREDIR) {
            int allnum = 1;
            for (wf *p = t.cmd; p; p = p->next)
              for (size_t i = 0; i < p->len; i++)
                if (p->word[i] < '0' || p->word[i] > '9') {
                  allnum = 0;
                  goto numchkdone;
                }
numchkdone:
            if (allnum) {
              int fd;
              redir *r;
              fd = 0;
              for (size_t i = 0; i < t.cmd->len; i++)
                fd = fd * 10 + (t.cmd->word[i] - '0');
              tokreset();
              if (!(r = parse_redir(n, fd)))
                return syntxerr(curline, "missing filename for", t.type);
              *tail = r;
              tail = &r->next;
              continue;
            }
          }
          if (t.cmd->flags & WFCMDSUB)
            cmdflags |= NECMDSUB;
          if (wc + 1 >= cap) {
            cap *= 2;
            streallocar(args, cap, wc, wf *);
          }
          args[wc++] = t.cmd;
          continue;
        }
      case TLP:
        tokreset();
        if (wc == 1 && !redirs) {
          sh_tok close;
          peektok(close, 0);
          if (close.type == TRP) {
            tokreset();
            if (!(body = parse_func()))
              return NULL;
            cmd_tree *n = st_alloc(sizeof(cmd_tree));
            n->type = FUNC;
            n->left = body;
            CFUNC(n) = args[0];
            n->right = NULL;
            n->flags = 0;
            n->line = curline;
            return n;
          }
          break;
        }
        /* fall through */
      default:
        if (t.type == TNOT)
          return synunexpected(curline, t);
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
  vc = get_assn(args, &sh_vars);
  l = newcmdnode(args, cmdflags, sh_vars, vc);
  if (redirs)
    return newredirnode(l, redirs);
  return l;
}

cmd_tree *
parse_andor(void)
{
  cmd_tree *l, *r;

  if (!(l = parse_pipe()))
    return NULL;
  for (;;) {
    sh_tok sep;
    peektok(sep, CHKALIAS | CHKKWD);
    if (sep.type != TAND && sep.type != TOR)
      return l;
    tokreset();
    chkwd |= CHKALIAS | CHKKWD | CHKNL;
    if (!(r = parse_pipe()))
      return NULL;
    if (eflag) {
      l->flags |= EFLAG_SAFE;
      if (l->right)
        l->right->flags |= EFLAG_SAFE;
    }
    l = newoppnode(sep.type, l, r);
  }
}

static cmd_tree *
parse_subsh(void)
{
  cmd_tree *sub;
  sh_tok t;
  tokreset();
  sub = parse_list(1);
  peektok(t, 0);
  if (t.type != TRP)
    return synexpected(curline, t, TRP);
  tokreset();
  chkwd |= CHKALIAS | CHKKWD;
  return sub;
}

cmd_tree *
parse_pipe(void)
{
  cmd_tree *cmd, *p;
  size_t n = 0;
  cmd_tree *stages[256];
  sh_tok t;
  int neg = 0;
  while (peektok(t, CHKALIAS | CHKKWD).type == TNOT) {
    tokreset();
    chkwd |= CHKALIAS | CHKKWD;
    neg++;
  }
  if (!(cmd = parse_cmd()))
    return NULL;
  stages[n++] = cmd;
  for (;;) {
    peektok(t, CHKALIAS | CHKKWD);
    if (t.type != TPIPE)
      break;
    tokreset();
    chkwd |= CHKALIAS | CHKKWD | CHKNL;
    if (!(p = parse_simple_cmd(0)))
      return NULL;
    stages[n++] = p;
  }
  cmd_tree *l;

  if (n>1) {
    l = st_alloc(sizeof(cmd_tree));
    l->type = OP;
    COPP(l) = TPIPE;
    l->flags = 0;
    l->line = curline;
    cmd_tree **list = st_alloc(n * sizeof(cmd_tree *));
    memcpy(list, stages, n * sizeof(cmd_tree *));
    CPIPE(l) = list;
    CPIPEC(l) = n;
  } else {
    l = cmd;
  }
  if (neg & 1)
    l->flags |= NEG;
  return l;
}

static cmd_tree *
parse_group(void)
{
  cmd_tree *body;
  sh_tok t;
  body = parse_list(1);
  peektok(t, 0);
  if (t.type != TRB)
    return synexpected(curline, t, TRB);
  tokreset();
  chkwd |= CHKALIAS | CHKKWD;
  return body;
}

static cmd_tree *
parse_func(void)
{
  sh_tok t;
  peektok(t, 0);
  if (t.type == TLB) {
    tokreset();
    return parse_group();
  }
  if (t.type == TLP) {
    return parse_subsh();
  }
  // Anything else is a syntax error
  return synunexpected(curline, t);
}

static redir *parse_redir(sh_tok rdr, int fd)
{
  sh_tok name;
  redir *r;

  nexttok(name, 0);
  if (name.type != TWORD)
    return NULL;

  r = st_alloc(sizeof(redir));
  r->type = rdr.sub;
  r->name = name.cmd;
  r->next = NULL;
  if (fd >= 0)
    r->fd = fd;
  else if (rdr.sub == RDHERE || rdr.sub == RDHERE_D)
    r->fd = 0;
  else
    r->fd = (rdr.sub == RDIN || rdr.sub == RDDUPI || rdr.sub == RDRW) ? 0 : 1;

  if (rdr.sub == RDHERE || rdr.sub == RDHERE_D) {
    r->heredoc_next = NULL;
    *heredoc_tail = r;
    heredoc_tail = &r->heredoc_next;
  }

  return r;
}

static void
parse_heredoc(void)
{
  if (!heredoc_head)
    return;

  redir *r;
  char *eofv;
  char *bpos;
  char c;
  size_t eofvlen;
  size_t bodylen;

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
        }
        if (c == '\n')
          break;
        stcheck(32), st_putc(c);
      }
      llen = stnext - lpos;
      if (llen == eofvlen && memcmp(lpos, eofv, eofvlen) == 0) {
        stunalloc(lpos);
        break;
      }
      stcheck(32), st_putc('\n');
    }

done:
    bodylen = stnext - bpos;
    r->heredoc = grab_str(bodylen);
  }
  heredoc_tail = &heredoc_head;
}

static cmd_tree *
parse_case(void)
{
  clause *clauses, *headcl, *tailcl;
  wf *word;
  sh_tok t;
  size_t cap, pc;

  tailcl = NULL;
  headcl = NULL;
  clauses = NULL;
  nexttok(t, CHKALIAS | CHKKWD);
  if (t.type != TWORD)
    return synunexpected(curline, t);
  word = t.cmd;

  nexttok(t, CHKALIAS | CHKKWD);
  if (t.type == TIN) {
    cap = WFCAP;
    for (;;) {
      chkwd = CHKNL | CHKKWD;
      tbuf.type = TNONE;
      peektok(t, 0);
      if (t.type == TESAC) {
        tokreset();
        break;
      }
      if (t.type == TLP) {
        tokreset();
        t = tokenize();
      }

      clauses = st_alloc(sizeof(clause));
      clauses->ptrn = st_alloc(cap * sizeof(wf *));
      clauses->next = NULL;
      clauses->body = NULL;
      pc = 0;

      for (;;) {
        peektok(t, 0);
        if (t.type == TWORD || t.type == TPIPE) {
          tokreset();
          if (t.type == TWORD) {
            if (pc >= cap) {
              cap *= 2;
              streallocar(clauses->ptrn, cap, pc, wf *);
            }
            clauses->ptrn[pc++] = t.cmd;
          }
          continue;
        }
        break;
      }

      if (!pc)
        return NULL;
      clauses->ptrn[pc] = NULL;
      peektok(t, 0);
      if (t.type != TRP)
        return synexpected(curline, t, TRP);
      tokreset();

      chkwd |= CHKALIAS | CHKKWD | CHKNL;
      clauses->body = parse_list(1);
      if (!headcl)
        headcl = clauses;
      else
        tailcl->next = clauses;
      tailcl = clauses;
      tbuf.type = TNONE;
      peektok(t, CHKNL | CHKKWD);
      if (t.type == TDSEMI) {
        tokreset();
        continue;
      }
      if (t.type == TESAC) {
        tokreset();
        break;
      }
      return synexpected(curline, t, TESAC);
    }
  } else {
    return synexpected(curline, t, TIN);
  }
  chkwd |= CHKALIAS | CHKKWD;

  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = CASE;
  CCASE(n).word = word;
  CCASE(n).clauses = headcl;
  n->flags = 0;
  n->line = curline;
  return n;
}

static cmd_tree *
parse_if(void)
{
  cmd_tree *cond, *then, *else_;
  sh_tok t;

  cond = parse_list(1);
  nexttok(t, CHKALIAS | CHKKWD);
  if (t.type != TTHEN)
    return synexpected(curline, t, TTHEN);
  then = parse_list(1);
  nexttok(t, CHKALIAS | CHKKWD);

  switch (t.type) {
    case TELIF:
      else_ = parse_if();
      break;
    case TELSE:
      else_ = parse_list(1);
      nexttok(t, CHKALIAS | CHKKWD);
      if (t.type != TFI)
        return synexpected(curline, t, TFI);
      break;
    case TFI:
      else_ = NULL;
      break;
    default:
      return synunexpected(curline, t);
  }

  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = IF;
  n->left = cond;
  n->right = then;
  CELSE(n) = else_;
  n->flags = 0;
  n->line = curline;
  return n;
}

cmd_tree *
parse_for(void)
{
  cmd_tree *body, *n;
  size_t wc, cap;
  wf *name, **words;
  sh_tok t;

  words = NULL;
  wc = 0;
  nexttok(t, CHKALIAS | CHKKWD);
  if (t.type != TWORD)
    return synunexpected(curline, t);
  name = t.cmd;

  nexttok(t, CHKALIAS | CHKKWD);
  if (t.type == TIN) {
    cap = WFCAP;
    chkwd = (chkwd & ~CHKALIAS) | CHKNL;
    words = st_alloc(cap * sizeof(wf *));
    for (;;) {
      peektok(t, 0);
      if (t.type != TWORD)
        break;
      tokreset();
      if (wc + 1 >= cap) {
        cap *= 2;
        streallocar(words, cap, wc, wf *);
      }
      words[wc++] = t.cmd;
    }
    words[wc] = NULL;
    if (!wc)
      return syntxerr(curline, "expected list before", TSEMI);
  }

  chkwd |= CHKALIAS | CHKKWD;
  if (t.type == TSEMI) {
    tokreset();
    nexttok(t, CHKALIAS | CHKKWD);
    if (t.type != TDO)
      return synexpected(curline, t, TDO);
  }

  if (!(body = parse_list(1)))
    return NULL;
  nexttok(t, CHKALIAS | CHKKWD);
  if (t.type != TDONE)
    return synexpected(curline, t, TDONE);

  /* new for loop node */
  n = st_alloc(sizeof(cmd_tree));
  n->type = FOR;
  CFOR(n).name = name;
  CFOR(n).words = words;
  n->right = body;
  n->flags = 0;
  n->line = curline;
  chkwd |= CHKALIAS | CHKKWD;
  return n;
}

cmd_tree *
parse_while(token tok)
{
  cmd_tree *condition, *body;
  sh_tok t;

  if (!(condition = parse_list(1)))
    return NULL;
  nexttok(t, CHKALIAS | CHKKWD | CHKNL);
  if (t.type != TDO)
    return synexpected(curline, t, TDO);
  do {
  peektok(t, CHKALIAS | CHKKWD | CHKNL);
  if (t.type != TNL)
    break;
  tokreset();
  }while (1);
  if (t.type == TSEMI)
    return synunexpected(curline, t);

  if (!(body = parse_list(1)))
    return NULL;
  nexttok(t, CHKALIAS | CHKKWD);
  if (t.type != TDONE)
    return synexpected(curline, t, TDONE);

  chkwd |= CHKALIAS | CHKKWD;

  cmd_tree *n = st_alloc(sizeof(cmd_tree));
  n->type = WHILE;
  n->left = condition;
  n->right = body;
  n->flags = (tok == TUNTIL) ? UNTIL : 0;
  n->line = shinpt->linenum;
  return n;
}

__attribute__((hot)) cmd_tree *
parse_cmd(void)
{
  cmd_tree *sub = NULL, *l;
  sh_tok t;

  peektok(t, 0);
  switch (t.type) {
    case TIF:
      tokreset();
      return parse_if();
    case TWHILE:
    case TUNTIL:
      tokreset();
      return parse_while(t.type);
    case TFOR:
      tokreset();
      return parse_for();
    case TLP:
      sub = parse_subsh();
      l = st_alloc(sizeof(cmd_tree));
      l->type = SUBSHELL;
      l->left = sub;
      l->flags = 0;
      l->line = curline;
      return l;
    case TLB:
      tokreset();
      l = st_alloc(sizeof(cmd_tree));
      l->type = BRACE;
      if (!(l->left = parse_group()))
        return NULL;
      l->flags = 0;
      l->line = curline;
      return l;
    case TCASE:
      tokreset();
      return parse_case();
    case TTHEN:
    case TELIF:
    case TELSE:
    case TFI:
    case TDONE:
      return NULL;
    default:
      return parse_simple_cmd(0);
  }
}

/* return the right syntax error message */
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
  return (t >= 0 && (size_t)t < arsz(toks) && toks[t]) ? toks[t] : "unknown";
}

char *
geterrline(int ln)
{
  char lnbuf[32], *line;
  size_t l = lltoa(ln, lnbuf);
  lnbuf[l] = '\0';
  line = st_strndup(lnbuf, l);
  return line;
}

static char *
errtok(sh_tok t)
{
  char *tok;
  if (t.type == TWORD && t.cmd)
    tok = join_wf(t.cmd);
  else
    tok = (char *)tokstr(t.type);
  return tok;
}

/* syntax warning error for unexpected tokens */
static void *
synunexpected(int ln, sh_tok wrong)
{
  const char *fn;

  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error: unexpected token \"%s\"\n",
            shname, geterrline(ln), fn, errtok(wrong));
  else
    fprintf(stderr, "%s: syntax error: unexpected token \"%s\"\n",
            shname, errtok(wrong));
  LSTATUS = 2;
  return NULL;
}

static void *
synexpected(int ln, sh_tok wrong, token t)
{
  const char *fn;

  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error:  found \"%s\" expected \"%s\"\n",
            shname, geterrline(ln), fn, errtok(wrong), tokstr(t));
  else
    fprintf(stderr, "%s: syntax error: found \"%s\" expected \"%s\"\n",
            shname, errtok(wrong), tokstr(t));
  LSTATUS = 2;
  return NULL;
}

/* syntax warning error for unexpected tokens */
static void *
syntxerr(int ln, char *msg, token t)
{
  const char *fn;

  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error: %s \"%s\"\n",
            shname, geterrline(ln), fn, msg, tokstr(t));
  else
    fprintf(stderr, "%s: syntax error: %s \"%s\"\n",
            shname, msg, tokstr(t));
  LSTATUS = 2;
  return NULL;
}
