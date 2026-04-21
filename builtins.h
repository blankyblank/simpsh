/* builtins.h - builtin shell commands */
#ifndef BUILTIN_H
#define BUILTIN_H

extern const char *builtins[];
extern int (* const builtin_funcs[])(char **);
int builtinnum(void);
void init_builtins(void);

/* vim: set filetype=c: */
#endif /* BUILTIN_H */
