/* simpsh.h - functions for running the shell global variables and declarations */
#ifndef SIMP_H
#define SIMP_H

#define _POSIX_C_SOURCE 200809L

#define defpath "/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"

/* functions for shell */
extern char *expand_ps1(char *);
extern char *lineread(char *);
extern void getbuildinfo(void);
#ifdef READLINE
extern void init_history(void);
#endif /* ifdef READLINE */

// NOTE: new
extern void simpsh_core(void);
extern char *read_accumulate(void);

#define simpsh_run(l) setinputstrn(l, strlen(l)); simpsh_core(); popinput();
#define sh_ccmd(cmd) setinputstrn(cmd, strlen(cmd)); simpsh_core(); exit(lstatus);
#define sh_script(fd) setinputf(fd); simpsh_core(); close(fd); exit(lstatus);
#define sh_stdin() setinputf(STDIN_FILENO); simpsh_core(); exit(lstatus);
extern void sh_interactive(void);

#endif /* SIMP_H */
