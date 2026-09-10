/* parse.c - parser functions */
/* NOLINT(build/c++11) */

#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */
#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "alloc.h"
#include "errmsg.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "parse.h"
#include "utils.h"

#define WFCAP 8
#define MAX_PDEPTH 512

static int pdepth;
redir *heredoc_head;
redir **heredoc_tail = &heredoc_head;
sh_tok tbuf = { .type = TNONE };
#define gettok(f) (chkwd = (f), tbuf = tokenize())

static int is_assn(wf *);
static int get_assn(wf **, wf *** restrict);
static cmd_tree *parse_andor(void);
static cmd_tree * parse_subsh(void);
static cmd_tree *parse_simple_cmd(void);
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
static void *syntxerrstr(int ln, char *msg, char *exp);

#define HDGROW() do { \
  if (hlen + 1 >= hcap) { \
    size_t nhcap = hcap ? hcap * 2 : 256; \
    hbuf = hbuf ? srealloc(hbuf, nhcap) : salloc(nhcap); \
    hcap = nhcap; \
  } \
} while (0)

#define HDPUTC(c) do { HDGROW(); hbuf[hlen++] = (c); llen++; } while (0)

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
  n->right = NULL;
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
  n->right = n->left = NULL;
  n->flags = flags;
  n->line = gstate.fnline ? gstate.lineno - gstate.fnline + 1 : gstate.lineno;
  return n;
}

void
collect_heredocs(void)
{
  if (heredoc_head)
    parse_heredoc();
}

/* check if word is name=value */
static int
is_assn(wf *cmd)
{
  if (cmd->qs != QNONE)
    return 0;
  char *eq;
  const char *p;

  if (!cmd->word)
    return 0;
  eq = memchr(cmd->word, '=', cmd->len);
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

__attribute__((hot)) cmd_tree *
parse_list(int multi)
{
  cmd_tree *l = NULL;

  if (!multi) {
    heredoc_head = NULL;
    heredoc_tail = &heredoc_head;
    pdepth = 0;
  } else if (++pdepth > MAX_PDEPTH) {
   return syntxerrstr(curline, "syntax error", "nesting too deep");
  }
  for (;;) {
    gettok(CHKALIAS | CHKKWD | (multi ? CHKNL : 0) | CHKBRACE);
    if (tbuf.type == TEOF) {
      if (multi)
        pdepth--;
      return NULL;
    }
    if (tbuf.type != TNL)
      break;
  }

  for (;;) {
    cmd_tree *r;

    if (!(r = parse_andor())) {
      if (PARSEERR) {
        heredoc_head = NULL;
        heredoc_tail = &heredoc_head;
        if (multi)
          pdepth--;
        return NULL;
      }
      if (multi)
        pdepth--;
      return l;
    }
    if (heredoc_head && tbuf.type == TEOF)
      parse_heredoc();

    if (tbuf.type == TBKGRND) {
      cmd_tree *bg;
      bg = newoppnode(TBKGRND, r, NULL);
      l = l ? newoppnode(TSEMI, l, bg) : newoppnode(TSEMI, bg, NULL);
      gettok(CHKALIAS | CHKKWD | (multi ? CHKNL : 0) | CHKBRACE);
      continue;
    }
    if (tbuf.type == TSEMI || (multi && tbuf.type == TNL)) {
      l = l ? newoppnode(TSEMI, l, r) : r;
      gettok(CHKALIAS | CHKKWD | (multi ? CHKNL : 0) | CHKBRACE);
      continue;
    }
    if (multi)
      pdepth--;
    return l ? newoppnode(TSEMI, l, r) : r;
  }
}

cmd_tree *
parse_simple_cmd(void)
{
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
  wc = cmdflags = 0;

  for (;;) {
    switch (tbuf.type) {
      case TREDIR:
        {
          redir *r;
          if (!(r = parse_redir(tbuf, -1)))
            return syntxerr(curline, "missing filename for", tbuf.type);
          *tail = r;
          tail = &r->next;
          gettok(0);
          continue;
        }
      case TWORD:
        {
          wf *name = tbuf.cmd;
          redir *r;
          gettok(0);

          if (tbuf.type == TREDIR && (name->flags & WFREDIRFD)) {
            unsigned long long u;
            int fd;
            if (name->len > 10)
              return syntxerrstr(curline, "bad file descriptor", "exceeds int max");
            fd = u = 0;
            for (size_t i = 0; i < name->len; i++) {
              u = u * 10 + (name->word[i] - '0');
              if (u > (unsigned long long)INT_MAX)
                return syntxerrstr(curline, "bad file descriptor", "larger than int max");
              fd = (int)u;
            }
            if (!(r = parse_redir(tbuf, fd)))
              return syntxerr(curline, "missing filename for", tbuf.type);
            *tail = r;
            tail = &r->next;
            gettok(0);
            continue;
          }

          if (name->flags & WFCMDSUB)
            cmdflags |= NECMDSUB;
          if (wc + 1 >= cap) {
            cap *= 2;
            streallocar(args, cap, wc, wf *);
          }
          args[wc++] = name;
          continue;
        }
      case TLP:
        if (wc == 1 && !redirs) {
          gettok(0);
          if (tbuf.type == TRP) {
            int b;
            b = gstate.fnline;
            gstate.fnline = curline;
            body = parse_func();
            gstate.fnline = b;
            if (!body)
              return NULL;
            cmd_tree *n;
            n = st_alloc(sizeof(cmd_tree));
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
        if (tbuf.type == TNOT)
          return synunexpected(curline, tbuf);
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
    if (tbuf.type != TAND && tbuf.type != TOR)
      return l;
    token op = tbuf.type;
    gettok(CHKALIAS | CHKKWD | CHKNL | CHKBRACE);
    if (!(r = parse_pipe()))
      return NULL;
    l->flags |= EFLAG_SAFE;
    if (l->right)
      l->right->flags |= EFLAG_SAFE;
    l = newoppnode(op, l, r);
  }
}

cmd_tree *
parse_cmdsub(void)
{
  cmd_tree *n = NULL;
  return n;
}

static cmd_tree *
parse_subsh(void)
{
  cmd_tree *n;
  n = parse_list(1);
  if (tbuf.type != TRP)
    return synexpected(curline, tbuf, TRP);
  if (PARSEERR)
    return NULL;
  gettok(0);
  return n;
}

static inline cmd_tree *
gettailredir(cmd_tree *c)
{
  redir *r, *redirs, **tail;
  redirs = NULL;
  tail = &redirs;

  for (;;) {
    if (tbuf.type == TWORD && (tbuf.cmd->flags & WFREDIRFD) && tbuf.cmd->word) {
      int fd = 0;
        fd = atoi_smpl(tbuf.cmd->word);
        gettok(0);
        if (!(r = parse_redir(tbuf, fd)))
          return syntxerr(curline, "missing filename for", tbuf.type);
        *tail = r, tail = &r->next;
        gettok(0);
        continue;
    }
    if (tbuf.type != TREDIR)
      break;
    if (!(r = parse_redir(tbuf, -1)))
      return syntxerr(curline, "missing filename for", tbuf.type);
    *tail = r, tail = &r->next;
    gettok(0);
  }
  if (redirs)
    c = newredirnode(c, redirs);
  return c;
}

cmd_tree *
parse_pipe(void)
{
  cmd_tree *cmd, *p;
  size_t n = 0;
  cmd_tree *stages[256];
  int neg = 0;

  while (tbuf.type == TNOT) {
    neg++;
    gettok(CHKALIAS | CHKKWD | CHKBRACE);
  }
  if (!(cmd = parse_cmd()))
    return NULL;
  if (!(cmd = gettailredir(cmd)))
    return NULL;
  stages[n++] = cmd;
  for (;;) {
    if (tbuf.type != TPIPE)
      break;
    if (n >= 256)
      return syntxerr(curline, "too many pipe stages", tbuf.type);
    gettok(CHKALIAS | CHKKWD | CHKNL | CHKBRACE);
    if (!(p = parse_cmd()))
      return NULL;
    p = gettailredir(p);
    stages[n++] = p;
  }
  for (size_t i = 0; i + 1 < n; i++)
    stages[i]->flags |= EFLAG_SAFE;

  cmd_tree *l;

  if (n > 1) {
    l = st_alloc(sizeof(cmd_tree));
    l->type = OP;
    COPP(l) = TPIPE;
    l->flags = 0;
    l->left = l->right = NULL;
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
  body = parse_list(1);
  if (tbuf.type != TRB)
    return synexpected(curline, tbuf, TRB);
  if (PARSEERR)
    return NULL;
  gettok(0);
  return body;
}

static cmd_tree *
parse_func(void)
{
  gettok(CHKNL | CHKBRACE);
  if (tbuf.type == TLB) {
    return parse_group();
  }
  if (tbuf.type == TLP) {
    return parse_subsh();
  }
  return synunexpected(curline, tbuf);
}

static redir *parse_redir(sh_tok rdr, int fd)
{
  redir *r;

  gettok(0);
  if (tbuf.type != TWORD)
    return NULL;

  r = st_alloc(sizeof(redir));
  r->type = rdr.sub;
  r->name = tbuf.cmd;
  r->next = NULL;
  r->heredoc = NULL;

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
  char *eofv, *dst, *hbuf, c;
  size_t eofvlen;
  size_t hlen, hcap, llen;

  hlen = hcap = llen = 0;
  hbuf = NULL;
  while (heredoc_head) {
    hlen = hcap = 0;
    hbuf = NULL;
    r = heredoc_head;
    heredoc_head = heredoc_head->heredoc_next;
    eofv = join_wf(r->name, 0);
    eofvlen = strlen(eofv);
    if (r->type == RDHERE_D) {
      c = shgetchar();
      while (c == '\t')
        c = shgetchar();
      shungetc(c);
    }

    for (;;) {
      if (r->type == RDHERE_D) {
        c = shgetchar();
        while (c == '\t')
          c = shgetchar();
        shungetc(c);
      }
      llen = 0;

      for (;;) {
        c = shgetchar();
        if (c == SHEOF) {
          if (llen == eofvlen && (!llen || !memcmp(hbuf + hlen - llen, eofv, eofvlen))) {
            hlen -= llen;
            goto done;
          }
          syntxerrstr(curline, "heredoc", "unexpected EOF while looking for delimiter");
          if (hbuf)
            sfree(hbuf);
          heredoc_head = NULL;
          heredoc_tail = &heredoc_head;
          return;
        }
        if (c == '\n') {
          shinpt->linenum++;
          break;
        }
        if (c == '\\' && btdepth > 0) {
          c = shgetchar();
          if (c == SHEOF) {
            HDPUTC('\\');
            continue;
          }
          if (c == '\n')
            shinpt->linenum++;
          HDPUTC('\\');
          HDPUTC(c);
          continue;
        }
        if (c == '`' && btdepth > 0) {
          if (llen == eofvlen && (!llen || !memcmp(hbuf + hlen - llen, eofv, eofvlen))) {
            hlen -= llen;
            shungetc(c);
            goto done;
          }
          syntxerrstr(curline, "heredoc", "unexpected EOF while looking for delimiter");
          if (hbuf)
            sfree(hbuf);
          heredoc_head = NULL;
          heredoc_tail = &heredoc_head;
          return;
        }
        HDPUTC(c);
      }
      if (llen == eofvlen && (!llen || !memcmp(hbuf + hlen - llen, eofv, eofvlen))) {
        hlen -= llen;
        break;
      }
      HDPUTC('\n');
    }

done:
    dst = st_alloc(hlen + 1);
    if (hlen)
      memcpy(dst, hbuf, hlen);
    dst[hlen] = '\0';
    r->heredoc = dst;
    if (hbuf)
      sfree(hbuf);
  }
  heredoc_tail = &heredoc_head;
}

static cmd_tree *
parse_case(void)
{
  clause *clauses, *headcl, *tailcl;
  wf *word;
  size_t cap, pc;

  tailcl = NULL;
  headcl = NULL;
  clauses = NULL;
  gettok(CHKALIAS | CHKKWD | CHKBRACE);
  if (tbuf.type != TWORD)
    return synunexpected(curline, tbuf);
  word = tbuf.cmd;

  gettok(CHKALIAS | CHKKWD | CHKBRACE);
  if (tbuf.type == TIN) {
    cap = WFCAP;
    for (;;) {
      gettok(CHKALIAS | CHKKWD | CHKNL | CHKBRACE);
      if (tbuf.type == TESAC) {
        break;
      }
      if (tbuf.type == TLP)
        gettok(CHKALIAS | CHKKWD | CHKBRACE);

      clauses = st_alloc(sizeof(clause));
      clauses->ptrn = st_alloc(cap * sizeof(wf *));
      clauses->next = NULL;
      clauses->body = NULL;
      pc = 0;

      for (;;) {
        if (tbuf.type == TWORD) {
          if (pc >= cap) {
            cap *= 2;
            streallocar(clauses->ptrn, cap, pc, wf *);
          }
          clauses->ptrn[pc++] = tbuf.cmd;
          gettok(0);
          continue;
        }
        if (tbuf.type == TPIPE) {
          gettok(0);
          continue;
        }
        break;
      }

      if (!pc)
        return NULL;
      clauses->ptrn[pc] = NULL;
      if (tbuf.type != TRP)
        return synexpected(curline, tbuf, TRP);
      if (PARSEERR)
        return NULL;

      clauses->body = parse_list(1);
      if (PARSEERR)
        return NULL;
      if (!headcl)
        headcl = clauses;
      else
        tailcl->next = clauses;
      tailcl = clauses;
      if (tbuf.type == TDSEMI) {
        continue;
      }
      if (tbuf.type == TESAC) {
        break;
      }
      return synexpected(curline, tbuf, TESAC);
    }
    gettok(CHKALIAS | CHKKWD | CHKBRACE);
  } else {
    return synexpected(curline, tbuf, TIN);
  }

  cmd_tree *n;
  n = st_alloc(sizeof(cmd_tree));
  n->type = CASE;
  CCASE(n).word = word;
  CCASE(n).clauses = headcl;
  n->flags = 0;
  n->left = n->right = NULL;
  n->line = curline;
  return n;
}

static cmd_tree *
parse_if(void)
{
  cmd_tree *cond, *then, *else_;

  if (!(cond = parse_list(1)))
    return syntxerrstr(curline, "expected", "command list"); // XXX: find out what the proper wording should be
  cond->flags |= EFLAG_SAFE;
  if (tbuf.type != TTHEN)
    return synexpected(curline, tbuf, TTHEN);
  then = parse_list(1);
  if (PARSEERR)
    return NULL;

  token tok = tbuf.type;
  switch (tok) {
    case TELIF:
      else_ = parse_if();
      if (PARSEERR)
        return NULL;
      break;
    case TELSE:
      else_ = parse_list(1);
      if (tbuf.type != TFI)
        return synexpected(curline, tbuf, TFI);
      if (PARSEERR)
        return NULL;
      gettok(CHKALIAS | CHKKWD | CHKBRACE);
      break;
    case TFI:
      else_ = NULL;
      gettok(CHKALIAS | CHKKWD | CHKBRACE);
      break;
    default:
      return synunexpected(curline, tbuf);
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

  words = NULL;
  wc = 0;
  gettok(CHKALIAS | CHKKWD | CHKBRACE);
  if (tbuf.type != TWORD)
    return synunexpected(curline, tbuf);
  name = tbuf.cmd;

  gettok(CHKALIAS | CHKKWD | CHKNL | CHKBRACE);
  if (tbuf.type == TIN) {
    cap = WFCAP;
    words = st_alloc(cap * sizeof(wf *));
    for (;;) {
      gettok(0);
      if (tbuf.type != TWORD)
        break;
      if (wc + 1 >= cap) {
        cap *= 2;
        streallocar(words, cap, wc, wf *);
      }
      words[wc++] = tbuf.cmd;
    }
    words[wc] = NULL;
  }

  if (tbuf.type == TNL)
    gettok(CHKALIAS | CHKKWD | CHKNL | CHKBRACE);
  if (tbuf.type == TSEMI) {
    gettok(CHKALIAS | CHKKWD | CHKNL);
    if (tbuf.type != TDO)
      return synexpected(curline, tbuf, TDO);
  }

  if (!(body = parse_list(1)))
    return NULL;
  if (PARSEERR)
    return NULL;
  if (tbuf.type != TDONE)
    return synexpected(curline, tbuf, TDONE);
  gettok(CHKALIAS | CHKKWD | CHKBRACE);

  /* new for loop node */
  n = st_alloc(sizeof(cmd_tree));
  n->type = FOR;
  CFOR(n).name = name;
  CFOR(n).words = words;
  n->right = body;
  n->left = NULL;
  n->flags = 0;
  n->line = curline;
  return n;
}

cmd_tree *
parse_while(token tok)
{
  cmd_tree *condition, *body;

  if (!(condition = parse_list(1)))
    return NULL;
  condition->flags |= EFLAG_SAFE;
  if (tbuf.type != TDO)
    return synexpected(curline, tbuf, TDO);
  if (tbuf.type == TSEMI)
    return synunexpected(curline, tbuf);

  if (!(body = parse_list(1)))
    return NULL;
  if (PARSEERR)
    return NULL;
  if (tbuf.type != TDONE)
    return synexpected(curline, tbuf, TDONE);
  gettok(CHKALIAS | CHKKWD | CHKBRACE);

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

  switch (tbuf.type) {
    case TIF:
      return parse_if();
    case TWHILE:
    case TUNTIL:
      return parse_while(tbuf.type);
    case TFOR:
      return parse_for();
    case TLP:
      sub = parse_subsh();
      l = st_alloc(sizeof(cmd_tree));
      l->type = SUBSHELL;
      l->left = sub;
      l->flags = 0;
      l->line = curline;
      l->right = NULL;
      if (PARSEERR)
        return NULL;
      return l;
    case TLB:
      l = st_alloc(sizeof(cmd_tree));
      l->type = BRACE;
      if (!(l->left = parse_group()))
        return NULL;
      l->flags = 0;
      l->right = NULL;
      l->line = curline;
      return l;
    case TCASE:
      return parse_case();
    case TTHEN:
    case TELIF:
    case TELSE:
    case TFI:
    case TDONE:
      return NULL;
    default:
      return parse_simple_cmd();
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
    tok = join_wf(t.cmd, 0);
  else
    tok = (char *)tokstr(t.type);
  return tok;
}

/* syntax warning error for unexpected tokens */
static void *
synunexpected(int ln, sh_tok wrong)
{
  const char *fn;

  if (PARSEERR)
    return NULL;
  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error: unexpected token \"%s\"\n",
            SHARGV0, geterrline(ln), fn, errtok(wrong));
  else
    fprintf(stderr, "%s: syntax error: unexpected token \"%s\"\n",
            SHARGV0, errtok(wrong));
  PARSEERR = 1;
  LSTATUS = 2;
  return NULL;
}

static void *
synexpected(int ln, sh_tok wrong, token t)
{
  const char *fn;

  if (PARSEERR)
    return NULL;
  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error:  found \"%s\" expected \"%s\"\n",
            SHARGV0, geterrline(ln), fn, errtok(wrong), tokstr(t));
  else
    fprintf(stderr, "%s: syntax error: found \"%s\" expected \"%s\"\n",
            SHARGV0, errtok(wrong), tokstr(t));
  PARSEERR = 1;
  LSTATUS = 2;
  return NULL;
}

/* syntax warning error for unexpected tokens */
static void *
syntxerr(int ln, char *msg, token t)
{
  const char *fn;

  if (PARSEERR)
    return NULL;
  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error: %s \"%s\"\n",
            SHARGV0, geterrline(ln), fn, msg, tokstr(t));
  else
    fprintf(stderr, "%s: syntax error: %s \"%s\"\n",
            SHARGV0, msg, tokstr(t));
  PARSEERR = 1;
  LSTATUS = 2;
  return NULL;
}

/* syntax warning error for unexpected tokens */
static void *
syntxerrstr(int ln, char *msg, char *exp)
{
  const char *fn;

  if (PARSEERR)
    return NULL;
  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error: %s \"%s\"\n",
            SHARGV0, geterrline(ln), fn, msg, exp);
  else
    fprintf(stderr, "%s: syntax error: %s \"%s\"\n",
            SHARGV0, msg, exp);
  PARSEERR = 1;
  LSTATUS = 2;
  return NULL;
}

