#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "errmsg.h"
#include "utils.h"

static int rval;
static char **garv;

int printesc(const char **, char *);
char *printstresc(const char *, size_t *, int *);
int printint(u64, int, int, int, char);
int printfloat(long double, int, int, int, char);

u64 getnum(void);
long double getfloat(void);
const char *getstr(void);
int getchr(void);

enum prntflg {
  PNEG = 1 << 0, /* - */
  PPOS = 1 << 1, /* + */
  PWS = 1 << 2,  /* ' ' */
  PPND = 1 << 3, /* # */
  PZRO = 1 << 4, /* 0 */
};

int
printesc(const char **sp, char *out)
{
  int val = 0;
  switch (**sp) {
    case 'a':
      (*sp)++;
      out[0] = '\a';
      return 1;
    case 'b':
      (*sp)++;
      out[0] = '\b';
      return 1;
    case 'c':
      (*sp)++;
      return -1;
    case 'e':
      (*sp)++;
      out[0] = '\033';
      return 1;
    case 'f':
      (*sp)++;
      out[0] = '\f';
      return 1;
    case 'n':
      (*sp)++;
      out[0] = '\n';
      return 1;
    case 'r':
      (*sp)++;
      out[0] = '\r';
      return 1;
    case 't':
      (*sp)++;
      out[0] = '\t';
      return 1;
    case 'v':
      (*sp)++;
      out[0] = '\v';
      return 1;
    case 'x':
      (*sp)++;
      for (int i = 0;i <= 1; i++) {
        if (isxdigit(**sp)) {
          val = val * 16 + hexval(*(*sp)++);
        } else {
          break;
        }
      }
      out[0] = val & 0xFF;
      return 1;
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      val = *(*sp)++ - '0';
      for (int i = 0; i <= 2; i++) {
        if (**sp >= '0' && **sp <= '7')
          val = val * 8 + (*(*sp)++ - '0');
        else
          break;
      }
      out[0] = val & 0xFF;
      return 1;
    case '\\':
      (*sp)++;
      out[0] = '\\';
      return 1;
    case '"':
      (*sp)++;
      out[0] = '\"';
      return 1;
    default:
      out[0] = *(*sp)++;
      return 1;
  }
  return 1;
}

char *
printstresc(const char *s, size_t *len, int *cesc)
{
  size_t slen, bufpos = 0;
  const char *sp;
  char *buf, out[4];
  int ret;

  slen = strlen(s);
  buf = st_alloc(slen + 1);

  sp = s;

  while (*sp) {
    if (*sp == '\\') {
      sp++;
      if ((ret = printesc(&sp, out)) < 0) {
        *len = bufpos, *cesc = 1;
        buf[bufpos] = '\0';
        return buf;
      }
      memcpy(buf + bufpos, out, ret);
      bufpos += ret;
    } else {
      buf[bufpos++] = *sp++;
    }
  }
  buf[bufpos] = '\0';
  *len = bufpos;
  return buf;
}

u64
getnum(void)
{
  if (!*garv)
    return 0;
  return strtoull(*(garv)++, NULL, 0);
}

long double
getfloat(void)
{
  if (!*garv)
    return 0;
  return strtold(*(garv)++, NULL);
}

const char *
getstr(void)
{
  if (!*garv)
    return "";
  return *(garv)++;
}

int
getchr(void)
{
  if (!*garv)
    return '\0';
  return *(*(garv)++);
}

int
printint(u64 n, int flags, int w, int prec, char cnv)
{
  char fmt[32], out[64];
  int pos = 0;

  fmt[pos++] = '%';
  if (flags & PNEG)
    fmt[pos++] = '-';
  if (flags & PPOS)
    fmt[pos++] = '+';
  else if (flags & PWS)
    fmt[pos++] = ' ';
  if (flags & PPND)
    fmt[pos++] = '#';
  if (flags & PZRO)
    fmt[pos++] = '0';
  if (w > 0)
    pos += snprintf(fmt + pos, sizeof(fmt) - pos, "%d", w);
  if (prec >= 0) {
    fmt[pos++] = '.';
    pos += snprintf(fmt + pos, sizeof(fmt) - pos, "%d", prec);
  }
  fmt[pos++] = 'l';
  fmt[pos++] = 'l';
  if (cnv == 'i')
    fmt[pos++] = 'd';
  else
    fmt[pos++] = cnv;
  fmt[pos] = '\0';
  if (cnv == 'd' || cnv == 'i')
    snprintf(out, sizeof(out), fmt, (long long)n);
  else
    snprintf(out, sizeof(out), fmt, n);
  if (fputs(out, shout) == EOF)
    rval = 1;
  return rval;
}

int
printfloat(long double n, int flags, int w, int prec, char cnv)
{
  char fmt[32], out[1024];
  int pos = 0;

  fmt[pos++] = '%';
  if (flags & PNEG)
    fmt[pos++] = '-';
  if (flags & PPOS)
    fmt[pos++] = '+';
  else if (flags & PWS)
    fmt[pos++] = ' ';
  if (flags & PPND)
    fmt[pos++] = '#';
  if (flags & PZRO)
    fmt[pos++] = '0';
  if (w > 0)
    pos += snprintf(fmt + pos, sizeof(fmt) - pos, "%d", w);
  if (prec >= 0) {
    fmt[pos++] = '.';
    pos += snprintf(fmt + pos, sizeof(fmt) - pos, "%d", prec);
  }
  fmt[pos++] = 'L';
  fmt[pos++] = cnv;
  fmt[pos] = '\0';
  snprintf(out, sizeof(out), fmt, n);
  if (fputs(out, shout) == EOF)
    rval = 1;
  return rval;
}

int
printfcmd(char **argv)
{
  char *fmt, *argv0;
  int parsed = 0;

  argv0 = *argv++;
  if (!*argv) {
    usage(argv0, helpmsgs[PRINTFH].usage);
    return 1;
  }
  fmt = *argv++;
  garv = argv;
  do {
    char out[4], *cp;
    int ret = 0;
    cp = fmt;
    while (*cp) {
      if (*cp == '\\') {
        cp++;
        if ((ret = printesc((const char **)&cp, out)) < 0)
          return 0;
        if (fwrite(out, 1, ret, shout) < (size_t)ret)
          rval = 1;
      } else if (*cp == '%') {
        int w = 0, prec = -1, flags = 0;
        char cc;
        cp++;
        while (*cp == '+' || *cp == '-' || *cp == ' ' || *cp == '#' ||
               *cp == '0') {
          if (*cp == '-')
            flags |= PNEG;
          if (*cp == '+')
            flags |= PPOS;
          if (*cp == ' ')
            flags |= PWS;
          if (*cp == '#')
            flags |= PPND;
          if (*cp == '0')
            flags |= PZRO;
          cp++;
        }
        if (*cp == '*') {
          w = (int)getnum();
          cp++;
        } else if (isdigit_(*cp)) {
          w = (int)strtoul(cp, &cp, 10);
        }
        if (*cp == '.') {
          cp++;
          if (*cp == '*') {
            prec = (int)getnum();
            cp++;
          } else if (isdigit_(*cp)) {
            prec = (int)strtoul(cp, &cp, 10);
          } else {
            prec = 0;
          }
        }
        switch (*cp) {
          case 'h':
            cp++;
            if (*cp == 'h')
              cp++;
            break;
          case 'l':
            cp++;
            if (*cp == 'l')
              cp++;
            break;
          case 'L':
            cp++;
            break;
        }
        parsed = 1;
        cc = *cp++;
        int i;
        switch (cc) {
          case 'd':
          case 'i':
          case 'u':
          case 'o':
          case 'x':
          case 'X':
            printint(getnum(), flags, w, prec, cc);
            break;
          case 'f':
          case 'F':
          case 'e':
          case 'E':
          case 'g':
          case 'G':
          case 'a':
          case 'A':
            printfloat(getfloat(), flags, w, prec, cc);
            break;
          case 's':
            {
              const char *s;
              size_t slen;
              s = getstr();
              slen = strlen(s);
              if (prec >= 0 && (size_t)prec < slen)
                slen = (size_t)prec;
              if (w > (int)slen && !(flags & PNEG))
                for (i = w - (int)slen; i > 0; i--)
                  if (fputc(' ', shout) == EOF)
                    rval = 1;
              if (fwrite(s, 1, slen, shout) < slen)
                rval = 1;
              if (w > (int)slen && (flags & PNEG))
                for (i = w - (int)slen; i > 0; i--)
                  if (fputc(' ', shout) == EOF)
                    rval = 1;
              break;
            }
          case 'c':
            {
              int ch;
              ch = getchr();
              if (w > 1 && !(flags & PNEG))
                for (i = w - 1; i > 0; i--)
                  if (fputc(' ', shout) == EOF)
                    rval = 1;
              if (fputc(ch, shout) == EOF)
                rval = 1;
              if (w > 1 && (flags & PNEG))
                for (i = w - 1; i > 0; i--)
                  if (fputc(' ', shout) == EOF)
                    rval = 1;
              break;
            }
          case 'b':
            {
              size_t blen;
              char *b;
              int cesc = 0;
              b = printstresc(getstr(), &blen, &cesc);
              if (cesc) {
                fwrite(b, 1, blen, shout);
                return 0;
              }
              if (fwrite(b, 1, blen, shout) < blen)
                rval = 1;
              break;
            }
          case '%':
            if (fputc('%', shout) == EOF)
              rval = 1;
            break;
          default:
            if (fputc('%', shout) == EOF)
              rval = 1;
            if (fputc(cc, shout) == EOF)
              rval = 1;
            break;
        }
      } else {
        if (fputc(*cp++, shout) == EOF)
          rval = 1;
      }
    }
  } while (*garv && parsed);
  return rval;
}
