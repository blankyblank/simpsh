/* error.h - error message macros/funcs */
#ifndef ERROR_H
#define ERROR_H

#include "alloc.h"
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>

#include "input.h"
#include "lex.h"
#include "main.h"
#include "utils.h"

static const char dmsg[] = "\nUse \"exit\" to leave the shell \n";

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
#define shwarn(b, m) warn("%s: %s", b, m)

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

/* return the right syntax error message */
#define parserr(l, e, m, t) ((iflag) ? lnsyntxerr(l, e, m, t) : syntxerr(m, t))

static const char *
tokstr(token t)
{
  static const char *toks[] = {
    [TWORD] = "word",   [TEOF] = "end of file",
    [TIF] = "if",       [TTHEN] = "then",
    [TELIF] = "elif",   [TELSE] = "else",
    [TFI] = "fi",       [TCASE] = "case",
    [TESAC] = "esac",   [TWHILE] = "while",
    [TUNTIL] = "until", [TFOR] = "for",
    [TIN] = "in",       [TDO] = "do",
    [TDONE] = "done",   [TNOT] = "!",
    [TPIPE] = "|",      [TAND] = "&&",
    [TOR] = "||",       [TSEMI] = ";",
    [TNL] = "newline",  [TLP] = "(",
    [TRP] = ")",        [TLB] = "{",
    [TRB] = "}",        [TBKGRND] = "&",
    [TDSEMI] = ";;",    [TREDIR] = "redirection",
    [TCMDSUB] = "$(",
  };
  return (t >= 0 && (size_t)t < arsz(toks, toks[0]) && toks[t]) ? toks[t] : "unknown";
}

static inline char *
geterrline(int ln)
{
  char lnbuf[32], *line;
  size_t l = lltoa(ln, lnbuf);
  lnbuf[l] = '\0';
  line = st_strndup(lnbuf, l);
  return line;
}

static inline char *
errtok(sh_tok t)
{
  char *tok;
  if (t.type == TWORD && t.cmd)
    tok = join_wf(t.cmd);
  else
    tok = (char *)tokstr(t.type);
  return tok;
}

/* syntax warning error for unexpected tokens */
static inline void *
synunexpected(int ln, sh_tok wrong)
{
  const char *fn;

  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error: unexpected token \"%s\"\n",
            shname, geterrline(ln), fn, errtok(wrong));
  else
    fprintf(stderr, "%s: syntax error: unexpected token \"%s\"\n",
            shname, errtok(wrong));
  lstatus = 2;
  return NULL;
}

static inline void *
synexpected(int ln, sh_tok wrong, token t)
{
  const char *fn;

  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error:  found \"%s\" expected \"%s\"\n",
            shname, geterrline(ln), fn, errtok(wrong), tokstr(t));
  else
    fprintf(stderr, "%s: syntax error: found \"%s\" expected \"%s\"\n",
            shname, errtok(wrong), tokstr(t));
  lstatus = 2;
  return NULL;
}

/* syntax warning error for unexpected tokens */
static inline void *
syntxerr(int ln, char *msg, token t)
{
  const char *fn;

  if ((fn = shinpt ? shinpt->name : NULL))
    fprintf(stderr, "%s: %s: %s: syntax error: %s \"%s\"\n",
            shname, geterrline(ln), fn, msg, tokstr(t));
  else
    fprintf(stderr, "%s: syntax error: %s \"%s\"\n",
            shname, msg, tokstr(t));
  lstatus = 2;
  return NULL;
}

#endif /* ERROR_H */
