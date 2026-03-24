#ifndef SIMP_H
#define SIMP_H

#define _GNU_SOURCE
#include <errno.h>
// #include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <readline/history.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

/*
 * maybe move these an some other variables to config.h
 * in the future if it makes sense
 */
#define MAX_CMDS 256
#define MAX_LENGTH 256

/* shell variables */
extern char **environ;
extern char *progname;
extern char *sh_argv0;
extern char **sh_argv;
extern int sh_argc;
extern int lstatus;
extern pid_t sh_pid;
extern char *sh_pid_s;
extern char *home;

/* functions for shell */
extern char *getpath(char **);
extern int shexec(char **);
extern char *lineread(void);
extern void init_history(void);

// vim: set filetype=c:
#endif /* SIMP_H */
