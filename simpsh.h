// vim: set filetype=c:
#ifndef SIMP_H
#define SIMP_H

#define _GNU_SOURCE
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <readline/history.h>
#include <readline/readline.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/* alias stuff */
#define ALIAS_BUCKETS 64

extern char **environ;

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};

extern alias *alias_tab[ALIAS_BUCKETS];

/* parser stuff */
typedef enum {
  AND,
  OR,
  SEMICOLON,
  NONE,
} cntrl;

/* store commands before putting them in ast */
typedef struct {
  char *cmd;
  cntrl op;
} cmd_tok;

/* tree struct to use for command parsing */
typedef struct cmd_tree cmd_tree;
struct cmd_tree {
  enum {
    CMD,
    OP
  } type;

  char **args;
  int c_false;
  cntrl op_t;
  cmd_tree *left;
  cmd_tree *right;
};

/* shell variables */
extern char *sh_argv0;
extern char **sh_argv;
extern int sh_argc;
extern int lstatus;
extern pid_t sh_pid;
extern char *sh_pid_s;

/* builtins */
int cdcmd(char **);
int echocmd(char **);
int execcmd(char **);
int exitcmd(char **);
int falsecmd(char **);
int helpcmd(char **);
int pwdcmd(char **);

/* functions for shell */
extern alias *get_alias(char *);
extern void set_alias(char *, char *);
extern void rm_alias(char *);
extern char **getinput(char *, char *);
extern int getbuiltin(char **);
extern char *getpath(char **);
extern int builtin_launch(char **);
extern int shexec(char **);
extern char *lineread(void);
extern char *exp_var(char *);
extern int scan_input(char *, cmd_tok *, int *);
extern cntrl chk_op(char *);
extern cmd_tree *build_tree(cmd_tok *, int, int);
extern int run_commands(cmd_tree *);

/* malloc and free (mostly free right now) or other small inlined functions */

static inline unsigned int
hash(const char *s) {
  unsigned int h = 0;
  while (*s) {
    h = h * 31 + (unsigned char)*s;
    s++;
  }
  return h % ALIAS_BUCKETS;
}


static inline void
freeptr(char **args) {
  if (args != NULL) {
    int i = 0;
    while (args[i] != NULL) {
      free(args[i]);
      i++;
    }
    free(args);
  }
}

static inline void
freetoks(cmd_tok *toks, int c) {
  int i;
  for (i = 0; i < c; i++) {
    if (toks[i].cmd != NULL)
      free(toks[i].cmd);
  }
}

#endif /* SIMP_H */
