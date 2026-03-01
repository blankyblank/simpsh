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

extern char **environ;
extern void freeptr(char **);
extern char **readinput(char *, char *);
extern int getbuiltin(char **);
extern int builtin_launch(char **);
extern int shexec(char **);
extern char *lineread(void);
extern char *getpath(char **);
extern char *getfullpath(char *, char *);
extern int startsWithSlash(const char *);

int cdcmd(char **);
int echocmd(char **);
int execcmd(char **);
int exitcmd(char **);
int falsecmd(char **);
int helpcmd(char **);
int pwdcmd(char **);

#endif /* SIMP_H */
