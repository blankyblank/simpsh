/* simpsh.h - functions for running the shell global variables and declarations */
#ifndef SIMP_H
#define SIMP_H

#define _POSIX_C_SOURCE 200809L
#include "input.h"
#include "main.h"
#include <sys/mman.h>

/* functions for shell */
extern char *lineread(char *);
extern void getbuildinfo(void);
#ifdef READLINE
extern void init_history(void);
#endif /* ifdef READLINE */

extern void simpsh_run(void);
extern int sh_interactive(void);

#define sh_ccmd(s) setinputstrn(s, strlen(s)); simpsh_run(); popinput();
#define sh_stdin() setinputf(STDIN_FILENO); simpsh_run(); popinput();
#define sh_script(i) setinputf(i); simpsh_run(); popinput(); close(i);

#endif /* SIMP_H */
