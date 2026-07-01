/* simpsh.h - functions for running the shell global variables and declarations */
#ifndef SIMP_H
#define SIMP_H

#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <sys/mman.h>

#include "input.h"
#include "main.h"

/* functions for shell */
extern char *lineread(int);
extern int eval_run(void);
extern void simpsh_run(void);
extern int sh_interactive(void);
extern void init_rc(int);

#ifdef READLINE
extern void init_history(void);
#endif /* ifdef READLINE */

#define sh_ccmd(s) setinputstrn(s, strlen(s)); simpsh_run(); popinput();
#define sh_stdin() setinputf(STDIN_FILENO, NULL, 0); simpsh_run(); popinput();
#define sh_script(i) setinputf(i, NULL, 0); simpsh_run(); popinput();

#endif /* SIMP_H */
