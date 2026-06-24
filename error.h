/* error.h - error message macros/funcs */
#ifndef ERROR_H
#define ERROR_H

#define _POSIX_C_SOURCE 200809L
#include <stddef.h>

#include "lex.h"
#include "main.h"

static const char shname[] = "simpsh";

/* err but it returns instead of exiting */
#define err(r, s) \
  { \
    perror(s); \
    return r; \
  }

/* errx but it returns instead of exiting */
#define errx(r, s) \
  { \
    fprintf(stderr, "%s\n", s); \
    return r; \
  }

/* error (returns doesn't exit) with simpsh: builtin: message: (errno message),
 * format */
/* be careful in if statements */
#define sherr(r, b, m) \
  warn("%s: %s", b, m); \
  return r

/* same as sherr  but doesn't return */
#define shwarn(b, m) warn("%s: %s: %s", shname, b, m)

/* warning with simpsh: builtin: message, format */
#define shwarnx(b, m) fprintf(stderr, "%s: %s: %s\n", shname, b, m)

/* error message with simpsh: builtin: arg: message, format */
#define shwarn_arg(b, a, m) \
  fprintf(stderr, "%s: %s: %s: %s\n", shname, b, a, m)

#define UFLAGMSG(v) fprintf(stderr, "%s: %s: unbound variable\n", shname, v)

/* unknow cli flag error message */
#define bad_opt(p, c) \
  (fprintf(stderr, "%s: %s: bad option %c\n", shname, p, c))

/*  missing required arguement error message */
#define no_opt(p, c) \
  (fprintf(stderr, "%s: %s: %c: requires arguement\n", shname, p, c))

#define arsz(a, o) (sizeof(a) / sizeof(o))

/* return the right syntax error message */
#define parserr(l, m, t) ((iflag) ? lnsyntxerr(l, m, t) : syntxerr(m, t))


#endif /* ERROR_H */
