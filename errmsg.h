/* error.h - error message macros/funcs */
#ifndef ERRMSG_H
#define ERRMSG_H
#define _POSIX_C_SOURCE 200809L

#include <stddef.h>

#include "config.h"

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
#if ENABLE_BASENAME
  BASENAMEH,
#endif /* ENABLE_BASENAME */
#if ENABLE_CAT
  CATH,
#endif /* ENABLE_CAT */
#if ENABLE_COMM
  COMMH,
#endif /* ENABLE_COMM */
#if ENABLE_CUT
  CUTH,
#endif /* ENABLE_CUT */
#if ENABLE_DIRNAME
  DIRNAMEH,
#endif /* ENABLE_DIRNAME */
#if ENABLE_EXPAND
  EXPANDH,
#endif /* ENABLE_EXPAND */
#if ENABLE_FOLD
  FOLDH,
#endif /* ENABLE_FOLD */
#if ENABLE_HEAD
  HEADH,
#endif /* ENABLE_HEAD */
#if ENABLE_PASTE
  PASTEH,
#endif /* ENABLE_PASTE */
#if ENABLE_READLINK
  READLINKH,
#endif /* ENABLE_READLINK */
#if ENABLE_REALPATH
  REALPATHH,
#endif /* ENABLE_REALPATH */
#if ENABLE_SLEEP
  SLEEPH,
#endif /* ENABLE_SLEEP */
#if ENABLE_SORT
  SORTH,
#endif /* ENABLE_SORT */
#if ENABLE_TAIL
  TAILH,
#endif /* ENABLE_TAIL */
#if ENABLE_TEE
  TEEH,
#endif /* ENABLE_TEE */
#if ENABLE_TR
  TRH,
#endif /* ENABLE_TR */
#if ENABLE_EXPAND
  UNEXPANDH,
#endif /* ENABLE_EXPAND */
#if ENABLE_UNIQ
  UNIQH,
#endif /* ENABLE_UNIQ */
#if ENABLE_WC
  WCH,
#endif /* ENABLE_WC */
  HELPCNT
} helpnum;

extern const builtinhelp helpmsgs[];
static const char dmsg[] = "\nUse \"exit\" to leave the shell \n";

#define UFLAGMSG(v) fprintf(stderr, "%s: %s: unbound variable\n", SHARGV0, (v))

/* unknow cli flag error message */
// check that the , 1 will work correctly
#define bad_opt(p, c) (fprintf(stderr, "%s: %s: bad option %c\n", SHARGV0, (p), (c)), 1)
#define bad_optx(c) fprintf(stderr, "%s: unknown option %c\n", SHARGV0, (c))

/*  missing required argument error message */
#define no_opt(p, c) (fprintf(stderr, "%s: %s: %c: requires argument\n", SHARGV0, (p), (c)))

#define usage(prog, usg) fprintf(stderr, "Usage: %s %s\n",(prog), (usg))
#define syntaxmsg(l, m) fprintf(stderr, "%s: %s: %s\n", SHARGV0, geterrline(l), (m))

extern char * geterrline(int);
extern int sherr(int r,const char *str,const char *msg);
extern int shwarn_arg(char *str,const char *arg,const char *msg);
extern int shwarn(const char *str,const char *msg);
extern int sherrx(int r, const char *msg);
extern void warn(const char *, ...);
__attribute__((noreturn)) void err(int eval, const char *fmt, ...);

#endif /* ERRMSG_H */
