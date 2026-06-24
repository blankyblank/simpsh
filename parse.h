/* parse.h - parser functions and declarations */
#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef struct redir redir;
typedef struct clause clause;
typedef struct cmd_tree cmd_tree;

/* AST node for commands */
struct cmd_tree {
  cmd_tree *left;
  cmd_tree *right;
  enum {
    CMD,
    OP,
    FUNC,
    SUBSHELL,
    BRACE,
    REDIR,
    IF,
    CASE,
    WHILE,
    FOR,
  } type;
  int flags;
  int line;
  union {
    struct { wf **args; size_t vc; wf **sh_vars; } cmd;
    struct { token op_t; } op;
    struct { redir *redirs;} redir;
    struct { wf *name; } func;
    struct { cmd_tree *else_; } if_;
    struct { wf *word; clause *clauses;} case_;
    struct { wf *name; wf **words; } for_;
  } t;
};

struct redir {
  redir *next;
  int fd;
  int type;
  wf *name;
  char *heredoc;
  redir *heredoc_next;
};

struct clause {
  wf **ptrn;
  cmd_tree *body;
  clause *next;
};

enum {
  NEG = 1 << 0,
  UNTIL = 1 << 1,
  EFLAG_SAFE = 1 << 2,
  NECMDSUB = 1 << 4,
};

#define CARGS(n) ((n)->t.cmd.args)
#define CVARS(n) ((n)->t.cmd.sh_vars)
#define CVARC(n) ((n)->t.cmd.vc)
#define COPP(n) ((n)->t.op.op_t)
#define CREDR(n) ((n)->t.redir.redirs)
#define CFUNC(n) ((n)->t.func.name)
#define CCASE(n) ((n)->t.case_)
#define CFOR(n) ((n)->t.for_)
#define CNEG(n) ((n)->flags & NEG)         /* check if NEGATE set */
#define CSAFE(n) ((n)->flags & EFLAG_SAFE) /* check if NEGATE set */
#define CELSE(n) ((n)->t.if_.else_)

/** build ast tree */
extern cmd_tree *parse_list(token s);

#endif /* PARSE_H */

