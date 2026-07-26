/* error.h - error message macros/funcs */
#ifndef ERRMSG_H
#define ERRMSG_H
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>

typedef struct {
  const char *name;
  const char *usage;
  const char *help;
} builtinhelp;

typedef enum {
  DOTH,
  LBRACKH,
  COLONH,
  ALIASH,
  BGH,
  BREAKH,
  CASEH,
  CDH,
  COMMANDH,
  CONTINUEH,
  ECHOH,
  EVALH,
  EXECH,
  EXITH,
  EXPORTH,
  FALSEH,
  FORH,
  FGH,
  GETOPTSH,
  HASHH,
  HELPH,
  IFH,
  JOBSH,
  KILLH,
  LOCALH,
  PWDH,
  PRINTFH,
  READH,
  READONLYH,
  RETURNH,
  SETH,
  SHIFTH,
  TESTH,
  TIMESH,
  TRAPH,
  TRUEH,
  TYPEH,
  ULIMITH,
  UMASKH,
  UNTILH,
  UNALIASH,
  UNSETH,
  WAITH,
  WHILEH,
  BRACEH,
#ifdef ENABLE_BASENAME
  BASENAMEH,
#endif /* ENABLE_BASENAME */
#ifdef ENABLE_CAT
  CATH,
#endif /* ENABLE_CAT */
#ifdef ENABLE_CUT
  CUTH,
#endif /* ENABLE_CUT */
#ifdef ENABLE_DIRNAME
  DIRNAMEH,
#endif /* ENABLE_DIRNAME */
#ifdef ENABLE_HEAD
  HEADH,
#endif /* ENABLE_HEAD */
#ifdef ENABLE_SLEEP
  SLEEPH,
#endif /* ENABLE_SLEEP */
#ifdef ENABLE_TAIL
  TAILH,
#endif /* ENABLE_TAIL */
  HELPCNT
} helpnum;

extern const builtinhelp helpmsgs[];
static const char dmsg[] = "\nUse \"exit\" to leave the shell \n";

#define UFLAGMSG(v) fprintf(stderr, "%s: %s: unbound variable\n", shname, (v))

/* unknow cli flag error message */
#define bad_opt(p, c) fprintf(stderr, "%s: %s: bad option %c\n", shname, (p), (c))
#define bad_optx(c) fprintf(stderr, "%s: unknown option %c\n", shname, (c))

/*  missing required argument error message */
#define no_opt(p, c) (fprintf(stderr, "%s: %s: %c: requires argument\n", shname, (p), (c)))

#define usage(prog, usg) fprintf(stderr, "Usage: %s %s\n",(prog), (usg))
#define syntaxmsg(l, m) fprintf(stderr, "%s: %s: %s\n", shname, geterrline(l), (m))

extern char * geterrline(int);
extern int sherr(int r,const char *str,const char *msg);
extern int shwarn_arg(char *str,const char *arg,const char *msg);
extern int shwarn(const char *str,const char *msg);
extern int sherrx(int r, const char *msg);
extern void warn(const char *, ...);
__attribute__((noreturn)) void err(int eval, const char *fmt, ...);

#endif /* ERRMSG_H */
