#ifndef SIMP_H
#define SIMP_H

#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <linux/limits.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

extern char **environ;
char **parseargs(char**, char*);
int getbuiltin(char**);
int builtin_launch(char**);
int shexec(char**);
int shell_loop(char**);
char *getpath(char**);
char *getfullpath(char *,char *);

int cdcmd(char**);
int pwdcmd(char**);
int falsecmd(char**);
int exitcmd(char**);

#endif /* SIMP_H */ 
