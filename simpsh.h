/* simpsh.h - functions for running the shell global variables and declarations */
#ifndef SIMP_H
#define SIMP_H

#include <fcntl.h>
#include <sys/mman.h>

#include "input.h"

/* functions for shell */
extern char *lineread(int);
extern int eval_run(void);
extern void simpsh_run(void);
extern int sh_interactive(void);
extern void init_rc(int);

#define sh_ccmd(s) setinputstrn(s, strlen(s)); simpsh_run(); popinput();
#define sh_stdin() setinputf(STDIN_FILENO, NULL, 0); simpsh_run(); popinput();
#define sh_script(i, n) setinputf(i, n, 0); simpsh_run(); popinput();

#endif /* SIMP_H */
