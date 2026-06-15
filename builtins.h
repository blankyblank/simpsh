/* builtins.h - builtin shell commands */
#ifndef BUILTIN_H
#define BUILTIN_H

#define BUILTIN_BUCKETS 32

extern const char *builtins[];
extern int (* const builtin_funcs[])(char **);
extern int builtin_tab[BUILTIN_BUCKETS];

extern int nbuiltins(void);
extern void init_builtins(void);

/* builtins */
extern int dotcmd(char **);
extern int cdcmd(char **);
extern int echocmd(char **);
extern int execcmd(char **);
extern int exitcmd(char **);
extern int falsecmd(char **);
extern int helpcmd(char **);
extern int pwdcmd(char **);
extern int truecmd(char **);

/* vim: set filetype=c: */
#endif /* BUILTIN_H */
