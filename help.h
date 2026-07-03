#ifndef TMPHELP_H
#define TMPHELP_H

typedef struct {
  const char *name;
  const char *usage;
  const char *help;
} builtinhelp;

extern const builtinhelp helpmsgs[];

extern int helpcmd(char **);

#endif /* TMPHELP_H */
