#ifndef PATH_H
#define PATH_H

#include <stddef.h>

#define CHASH_MAX 64
typedef struct cmdent {
  char *name;
  char *path;
} cmdent;

extern cmdent chash[CHASH_MAX];
extern unsigned int chashn;

extern void initchash(void);
extern char *findchash(const char *);
extern void setchash(const char *restrict,const char *restrict);
extern void rmchash(const char *);
extern char *chkpath(const char *restrict, const char *restrict, int, unsigned int);
extern char *getpath(char *);
extern int hashcmd(char **);

#endif /* PATH_H */
