/* simpsh.h - functions for running the shell global variables and declarations */
#ifndef SIMP_H
#define SIMP_H

#define _POSIX_C_SOURCE 200809L
#include "input.h"
#include "main.h"
#include <sys/mman.h>

/* functions for shell */
extern char *lineread(char *);
extern void simpsh_run(void);
extern int sh_interactive(void);

#ifndef MUSL
extern void getbuildinfo(void);
#endif /* ifndef MUSL */
#ifdef READLINE
extern void init_history(void);
#endif /* ifdef READLINE */

#define sh_ccmd(s) setinputstrn(s, strlen(s)); simpsh_run(); popinput();
#define sh_stdin() setinputf(STDIN_FILENO, 0); simpsh_run(); popinput();
#define sh_script(i) setinputf(i, 0); simpsh_run(); popinput();

#endif /* SIMP_H */
