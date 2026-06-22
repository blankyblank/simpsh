/* parse.h - parser functions and declarations */
#ifndef PARSE_H
#define PARSE_H

#include "lex.h"

typedef struct redir redir;
struct redir {
  redir *next;
  int fd;
  int type;
  wf *name;
  char *heredoc;
  redir *heredoc_next;
};

/*
 * AST node for commands
 * @field node left
 * @field node right
 * @field enum node type
 * @field union of ast structs
 * @field enum node type
 * @field word fragment array args
 * @field int flags (bg, negate)
 */
typedef struct cmd_tree cmd_tree;
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
    WHILE,
    FOR,
  } type;
  int flags;
  union {
    struct { wf **args; size_t vc; wf **sh_vars; } cmd;
    struct { token op_t; } op;
    struct { redir *redirs;} redir;
    struct { wf *name; } func;
    struct { cmd_tree *else_; } if_;
    struct { wf *name; wf **words; } for_;
  } t;
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
#define CFOR(n) ((n)->t.for_)
#define CNEG(n) ((n)->flags & NEG)         /* check if NEGATE set */
#define CSAFE(n) ((n)->flags & EFLAG_SAFE) /* check if NEGATE set */
#define CELSE(n) ((n)->t.if_.else_)

/** build ast tree */
extern cmd_tree *parse_list(token s);

#endif /* PARSE_H */

