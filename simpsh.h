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

#define MAX_CMDS 256
#define MAX_LENGTH 256
/* alias stuff */
#define ALIAS_BUCKETS 64
#define MAX_ALIAS_DEPTH 10

extern char **environ;

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};

extern alias *alias_tab[ALIAS_BUCKETS];

/* shell variables */
extern char *progname;
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
int unaliascmd(char **);

/* functions for shell */
extern int create_histfile(char *, char *);
extern alias *lookup_alias(char *);
extern void set_alias(char *, char *);
extern void rm_alias(char *);
extern int getbuiltin(char **);
extern char *getpath(char **);
extern int builtin_launch(char **);
extern int shexec(char **);
extern char *lineread(void);
extern char *exp_var(char *, size_t *, size_t *);
extern char *exp_var_old(char *);

static inline unsigned int
hash(const char *s) {
  unsigned int h = 0;
  while (*s) {
    h = h * 31 + (unsigned char)*s;
    s++;
  }
  return h % ALIAS_BUCKETS;
}

// vim: set filetype=c:
#endif /* SIMP_H */
