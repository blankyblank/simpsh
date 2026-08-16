#ifndef PIPE_H
#define PIPE_H

#include "builtins.h"
#include "exec.h"
#include "main.h"
#include "parse.h"
#include "var.h"

#define MAXFVARS 64
#define NRLIM 9

typedef struct  {
  unsigned long long cur;
  unsigned long long max;
} fkrlimit;

typedef struct {
  int cwd;               /* saved cwd */
  tmp_var *vars;         /* saved variables */
  size_t varc;           /* saved variable count */
  int varcap;            /* current saved var cap */
  unsigned opts;         /* saved shell options */
  unsigned optsv : 1;    /* have saved shell options */
  char *argv0;           /* saved argv[0] */
  char **argv;           /* saved argv */
  int argc;              /* saved argc */
  int argvalloc;         /* saved argv allocated flag */
  unsigned pparamsv : 1; /* have saved positional params */
  int umask;             /* saved umask */
  unsigned umasksv : 1;  /* have saved umask*/
  tmp_var *alias;        /* saved aliases */
  size_t aliasc;         /* saved alias count */
  int aliascap;          /* current saved alias cap */
  char **trap;           /* saved traps */
  unsigned long trapm;   /* saved trap mask */
  unsigned trapsv;       /* have saved traps */
  fkrlimit *rlim;        /* saved resouce limits */
  unsigned rlimsv;       /* have saved rlimits */
} fakestate;

extern int fakectx;
extern fakestate *fkstate;

void fkrestore(fakestate *);
void fkinit(fakestate *);
void svfkcwd(fakestate *);
void svfkvar(fakestate *, const char *);
void svfkalias(fakestate *, const char *);
void svfkopts(fakestate *);
void svfkargv(fakestate *);
void savefkulimit(fakestate *, int, u64, u64);
void svfkumask(fakestate *);
void svfktraps(fakestate *, int);

static inline int
canfakepipe(cmd_tree *n)
{
  if (n->type == REDIR)
    n = n->left;
  if (n->type != CMD)
    return 0;
  const builtin *bi;

  bi = findbuiltin(CARGS(n)[0]->word);
  if (bi && bi->fn != &execcmd &&
      bi->fn != &evalcmd && bi->fn != &commandcmd && bi->fn != &dotcmd)
    return 1;
  return 0;
}

static inline int
canfakesubsh(const cmd_tree *n)
{
  if (!n)
    return 0;
  switch (n->type) {
    case CMD:
      if (CARGS(n) && CARGS(n)[0]) {
        char *w = CARGS(n)[0]->word;
        if (CARGS(n)[0]->len == 4 && w[0] == 'e' && w[1] == 'x' && w[2] == 'e' && w[3] == 'c')
          return 0;
      }
      return 1;
    case FOR:
      return canfakesubsh(n->right);
    case FUNC:
      return 1;
    case CASE:
      for (clause *c = CCASE(n).clauses; c; c = c->next)
        if (!canfakesubsh(c->body))
          return 0;
      return 1;
    case OP:
      if (COPP(n) == TPIPE) {
        for (size_t i = 0; i < CPIPEC(n); i++)
          if (!canfakesubsh(CPIPE(n)[i]))
            return 0;
        return 1;
      }
      return canfakesubsh(n->left) && canfakesubsh(n->right);
      /* fall through */
    case REDIR:
    case SUBSHELL:
    case BRACE:
      return canfakesubsh(n->left);
    case IF:
      return canfakesubsh(n->left) && canfakesubsh(n->right) && (!CELSE(n) || canfakesubsh(CELSE(n)));
    case WHILE:
      return canfakesubsh(n->left) && canfakesubsh(n->right);
    default:
  return 0;
  }
}

#endif /* PIPE_H */
