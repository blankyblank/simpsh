/* expand.c - variable/string expandsion logic */
#define _POSIX_C_SOURCE 200809L
#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "arith.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "malloc.h"
#include "opts.h"
#include "parse.h"
#include "utils.h"
#include "var.h"

static char **splitword(char *restrict, size_t, size_t, ifssect *restrict, size_t *restrict);

/** get the pid shell variable */
#define varpid() (st_strdup(sh_pid_s))
#define varbgpid() (st_strdup(sh_bgpid_s))
#define varargc() \
  char *buf; \
  buf = st_alloc(16); \
  *olen = lltoa(sh_argc, buf); \
  return buf;

/** get the variable for the return status of last command */
static inline char *
varstatus(size_t *o)
{
  size_t n, i, e;
  char *buf, t;

  i = 0;
  n = lstatus;
  buf = st_alloc(4);
  do {
    buf[i++] = (n % 10) + '0';
    n /= 10;
  } while (n);
  e = i - 1;
  for (size_t s = 0; s < e; s++) {
    t = buf[s];
    buf[s] = buf[e];
    buf[e] = t;
    e--;
  }
  buf[i] = '\0';
  *o = i;
  return buf;
}

/** get positional parameters */
static inline char *
get_posparam(int n)
{
  if (n == 0)
    return sh_argv0 ? sh_argv0 : "";

  if (n < 0 || n > sh_argc)
    return "";
  return sh_argv[n - 1] ? sh_argv[n - 1] : "";
}

/** find if variable is positional parameter */
static inline int
is_posparam(const char *var, size_t var_l)
{
  size_t i;
  if (!var || !*var)
    return 0;
  for (i = 0; i < var_l; i++) {
    if (!isdigit_(var[i]))
      return 0;
  }
  return 1;
}

char *
homedir(char *user)
{
  char e[PATH_MAX];
  FILE *pw;
  size_t i;

  if (!user)
    return NULL;
  if (!(pw = fopen("/etc/passwd", "r")))
    return NULL;
  while (fgets(e, PATH_MAX, pw)) {
    char *s, *end;
    size_t ulen;

    if (e[0] == '#' || e[0] == '\n')
      continue;
    s = e;
    end = strchr(s, ':');
    if (!end) 
      continue;
    ulen = end - s;
    if (strncmp(s, user, ulen) == 0 && s[ulen] == ':') {
      s += ulen + 1;
      for (i = 0; i < 4; i++) {
        s = strchr(s, ':');
        if (!end)
          break;
        s++;
      }
      if (i != 4)
        continue;
      end = strchr(s, ':');
      if (end)
        *end = '\0';
      if (!*s)
        continue;
      fclose(pw);
      return st_strdup(s);
    } 
  }
  fclose(pw);
  return NULL;
}

char *
exp_tilde(char *restrict word, size_t s, size_t *restrict e, size_t *restrict olen)
{
  char *hm, strt;
  size_t end;

  if (word[s] != '~')
    return NULL;
  if (s == 0 && (word[s + 1] == '\0' || word[s + 1] == '/')) {
    if (!(hm = getvar("HOME")))
      return NULL;
    *e = s + 1;
    *olen = strlen(hm);
    return st_strdup(hm);
  } else {
    end = s + 1;
    while (word[end] && word[end] != '/')
      end++;
    strt = word[end];
    word[end] = '\0';
    hm = homedir(word + s + 1);
    word[end] = strt;
    if (hm) {
      *e = end;
      *olen = strlen(hm);
      return hm;
    }
  }
  return NULL;
}

/** take in ps1 char * to do variable expansion for prompt */
char *
expand_ps1(char *p)
{
  size_t i, varlen, cbrace, flen;
  char *s, *val = NULL;
  char f[4096], vcpy[256];

  if (!p)
    return NULL;

  varlen = 0;
  flen = 0;
  i = 0;
  while (p[i]) {
    if (p[i] == '$') {
      if (p[i + 1] == '$' || p[i + 1] == '?') {
        varlen = 2;
        vcpy[0] = p[i];
        vcpy[1] = p[i + 1];
        vcpy[2] = '\0';
      } else if (p[i + 1] == '{') {
        cbrace = i + 2;
        while (p[cbrace] && p[cbrace] != '}')
          cbrace++;
        if (!p[cbrace]) {
          f[flen++] = p[i++];
          continue;
        }
        varlen = cbrace - (i + 2);
        vcpy[0] = '$';
        memcpy(vcpy + 1, p + i + 2, varlen);
        vcpy[varlen + 1] = '\0';
        varlen = cbrace - i + 1;
      } else if (isalnum_(p[i + 1])) {
        varlen = i + 1;
        while (isalnum_(p[varlen]))
          varlen++;
        varlen -= i;
        memcpy(vcpy, p + i, varlen);
        vcpy[varlen] = '\0';
      } else {
        if (p[i + 1] == ' ' || p[i + 1] == '\t' || p[i + 1] == '\0') {
          f[flen++] = lstatus == 0 ? '$' : 'X';
          i++;
          continue;
        }
        f[flen++] = p[i++];
        continue;
      }

      size_t vlen = 0;
      if (vcpy[0] == '$' && vcpy[1]) {
        switch (vcpy[1]) {
          case '$':
            val = varpid();
            vlen = strlen(val);
            break;
          case '?':
            val = varstatus(&vlen);
            break;
          case '!':
            val = varbgpid();
            vlen = strlen(val);
            break;
          case '#':
            {
              char buf[16];
              vlen = lltoa(sh_argc, buf);
              val = buf;
              break;
            }
          default:
            if (isdigit_(vcpy[1])) {
              int n = atoi(vcpy + 1);
              val = get_posparam(n);
              vlen = val ? strlen(val) : 0;
            } else {
              shvar *v = findvar_n(vcpy + 1, strlen(vcpy + 1));
              if (v) {
                val = shvar_val(v);
                vlen = strlen(val);
              }
            }
        }
      }

      if (val) {
        for (s = val; *s; s++)
          f[flen++] = *s;
      }
      i += varlen;
    } else {
      f[flen++] = p[i++];
    }
  }
  f[flen] = '\0';
  s = st_alloc(flen + 1);
  memcpy(s, f, flen + 1);
  return s;
}

/** expand argument vector */
char **
expand_argv(wf **args, size_t *restrict t)
{
  ifssect *ifsexp;
  size_t ifsn, cap, fargc, tlen;
  size_t i, argc, elen;
  char **fargv, **argv;

  *t = 0;
  cap = 0;
  fargc = 0;
  argc = 0;
  fargv = NULL;
  ifsexp = NULL;

  while (args[argc])
    argc++;
  for (size_t i = 0; i < argc; i++)
    cap += args[i]->len;
  argv = st_alloc((cap + 1) * sizeof(char *));

  for (i = 0; i < argc; i++) {
    char *expanded;
    if (!(expanded = exp_word(args[i], &ifsn, &ifsexp,  &elen))) {
      return NULL;
    }
    fargv = splitword(expanded, elen, ifsn, ifsexp, &tlen);
    *t += tlen;
    for (int j = 0; fargv[j]; j++)
      argv[fargc++] = fargv[j];
  }
  argv[fargc] = NULL;
  return argv;
}

/** expand word with variable substitution */
__attribute__((hot)) char *
exp_word(wf *wordf, size_t *restrict n, ifssect **restrict ifssects, size_t *restrict rlen)
{
  size_t end = 0, len = 0, si = 0, nsplits = 0;
  size_t sectstart, i;
  ifssect *sects;
  wf *f;
  char *expanded;

  for (f = wordf; f; f = f->next) {
    if (f->qs == QNONE || f->qs == QCMDSUB)
      nsplits++;
  }
  if (nsplits > 0 && ifssects) {
    sects = st_alloc(nsplits * sizeof(ifssect));
    *ifssects = sects;
  } else {
    sects = NULL;
    if (ifssects)
      *ifssects = NULL;
  }

  for (f = wordf; f; f = f->next) {
    sectstart = len;
    switch (f->qs) {
      case QDOUBLE:
      case QSINGLE:
        st_write(f->word, f->len, len);
        break;
      case QNONE:
        i = 0;
        while (i < f->len) {
          if (f->word[i] == '~') {
            size_t elen = 0;
            expanded = exp_tilde(f->word, i, &end, &elen);
            if (expanded) {
              st_write(expanded, elen, len);
              i = end;
            } else {
              st_putc('~');
              len++;
              i++;
            }
          } else {
            st_putc(f->word[i]);
            len++;
            i++;
          }
        }
        if (sects)
          sects[si++] = (ifssect) {
            sectstart,
            len - sectstart,
          };
        break;
      case QARITH:
        {
          long long val;
          char buf[32];
          size_t rlen;
          stmark arithm;

          arithm = stack_mark();
          val = arith_eval(f->word, f->len);
          stack_restore(arithm);
          rlen = lltoa(val, buf);
          st_write(buf, rlen, len);
          break;
        }
      case QCMDSUB:
      case QCMDSUB_DQ:
        {
          cmd_tree *cmdsub, *cmdsdup;
          int sublen;
          char *cmdsubpos;
          size_t savesl;

          savesl = stleft;
          cmdsubpos = stnext;
          setinputstrn(f->word, f->len);
          last_tok = SHTOK(TNONE);
          chkwd = 0;
          notclosed = 0;
          cmdsub = parse_list(TEOF, 1);

          popinput();
          cmdsdup = tree_dup(cmdsub);
          if (!cmdsdup)
            return NULL;
          stnext = cmdsubpos;
          stleft = savesl;
          if ((sublen = run_cmdsub(cmdsdup)) < 0) {
            free_tree(cmdsdup);
            return NULL;
          }
          len += sublen;
          free_tree(cmdsdup);
          if (f->qs == QCMDSUB && sects)
            sects[si++] = (ifssect) {
              sectstart,
              len - sectstart,
            };
          break;
        }
      case QVAR:
      case QVAR_DQ:
      case QBRACE:
      case QBRACE_DQ: {
        char *val = NULL;
        char buf[16];
        size_t vlen = 0;

        if (f->len == 1) {
          switch (f->word[0]) {
            case '$':
            val = varpid();
            vlen = strlen(val);
            break;
            case '?':
            val = varstatus(&vlen);
            break;
            case '!':
            val = varbgpid();
            vlen = strlen(val);
            break;
            case '#': {
              vlen = lltoa(sh_argc, buf);
              val  = buf;
              break;
            }
          }
        }
        if (is_posparam(f->word, f->len)) {
          int n = 0;
          for (size_t i = 0; i < f->len; i++)
            n = n * 10 + (f->word[i] - '0');
          val = get_posparam(n);
          vlen = val ? strlen(val) : 0;
        } else {
          shvar *v = findvar_n(f->word, f->len);
          if (v) {
            val = shvar_val(v);
            // vlen = v->flen - (v->nlen + 2);
            vlen = strlen(val);
          } else if (uflag && f->len > 0) {
            char name[64];
            nmemcpy(name, f->word, f->len);
            UFLAGMSG(name);
            nounseterr = 1;
          }
        }
        if (val) {
          st_write(val, vlen, len);
          if ((f->qs == QVAR || f->qs == QBRACE) && sects)
            sects[si++] = (ifssect) { sectstart, len - sectstart };
        }
        break;
        }
    }
  }
  if (n)
    *n = si;
  if (rlen)
    *rlen = len;
  return grab_str(len);
}

static char **
splitword(char *restrict str, size_t slen, size_t nsect, ifssect *restrict sects, size_t *restrict tlen)
{
  enum {
    M_IMMUNE,
    M_IFSWS,
    M_IFSN,
    M_NORMAL
  };

  char *ifs;
  char **fargv;
  size_t fargc;
  int mode;

  ifs = getvar("IFS");
  if (!ifs)
    ifs = " \t\n";
  if (tlen)
    *tlen = 0;

  if (!*ifs || !nsect) {
    fargv = st_alloc(2 * sizeof(char *));
    fargv[0] = str;
    fargv[1] = NULL;
    return fargv;
  }
  fargv = st_alloc((slen + 1) * sizeof(char *));

  size_t sidx, pos;
  char *field;

  fargc = 0;
  sidx = 0;
  pos = 0;
  field = NULL;

  while (pos < slen) {
    int insect;

    while (sidx < nsect && pos >= sects[sidx].start + sects[sidx].len)
      sidx++;
    insect = (sidx < nsect && pos >= sects[sidx].start);
    if (!insect)
      mode = M_IMMUNE;
    else if (is_ws(str[pos]))
      mode = M_IFSWS;
    else if (is_ifs_nws(str[pos], ifs))
      mode = M_IFSN;
    else
      mode = M_NORMAL;

    switch (mode) {
      case M_IMMUNE:
        if (!field)
          field = str + pos;
        pos++;
        continue;
      case M_IFSWS:
        if (field) {
          str[pos++] = '\0';
          if (tlen)
            *tlen += (str + pos) - field;
          fargv[fargc++] = field;
          field = NULL;
        } else {
          pos++;
        }
        continue;
      case M_IFSN:
        str[pos] = '\0';
        if (field) {
          if (tlen)
            *tlen += (str + pos) - field;
          fargv[fargc++] = field;
        } else
          fargv[fargc++] = str + pos;
        pos++;
        field = NULL;
        continue;
      case M_NORMAL:
      if (!field) {
          field = str + pos;
      }
      pos++;
      continue;
    }
  }

  if (field) {
    if (tlen)
      *tlen += slen - (field - str);
    fargv[fargc++] = field;
  }
  fargv[fargc] = NULL;
  return fargv;
}

