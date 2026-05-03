/* simpsh.h - functions for running the shell global variables and declarations */
#ifndef SIMP_H
#define SIMP_H

#define _POSIX_C_SOURCE 200809L

#define defpath "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

/* functions for shell */
extern char *getpath(char *);
extern char *lineread(void);
extern void init_history(void);
extern void getbuildinfo(void);

/* vim: set filetype=c: */
#endif /* SIMP_H */
