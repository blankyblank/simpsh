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
#include "glob.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "alloc.h"
#include "opts.h"
#include "parse.h"
#include "utils.h"
#include "var.h"

int ifsnull = 0;
static char ifschar[256];

static wf **splitword(wf *restrict, size_t *restrict);
static char *exp_str(char *restrict, size_t, size_t *restrict);

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

void
ifsupdt(const char *ifs)
{
  memset(ifschar, 0, 256);
  if (!ifs)
    ifs = " \t\n";
  ifsnull = !*ifs;
  for (; *ifs; ifs++)
    if (!is_ws(*ifs))
      ifschar[(unsigned char)*ifs] = 1;
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

static char *
exp_str(char *restrict str, size_t slen, size_t *restrict outlen)
{
  size_t i = 0, vlen, end, tlen = 0;
  char buf[16], *val;

  while (i < slen) {
    size_t s = i;
    while (i < slen && str[i] != '$')
      i++;
    if (i > s)
      st_write(str + s, i - s, tlen);
    if (i >= slen)
      break;
    vlen = 0;
    end = i;
    val = NULL;
    switch (str[i + 1]) {
      case '$':
        val = varpid();
        vlen = strlen(val);
        end = i + 2;
        break;
      case '?':
        val = varstatus(&vlen);
        end = i + 2;
        break;
      case '!':
        val = varbgpid();
        vlen = strlen(val);
        end = i + 2;
        break;
      case '#':
        vlen = lltoa(sh_argc, buf);
        val = buf;
        end = i + 2;
        break;
      case '{':
        {
          size_t j, vtl;
          shvar *v;
          j = i + 2;
          while (j < slen && str[j] != '}')
            j++;
          if (j < slen) {
            vtl = j - (i + 2);
            if (vtl > 0) {
              v = findvar_n(str + i + 2, vtl);
              if (v) {
                val = shvar_val(v);
                vlen = strlen(val);
              }
            }
            end = j + 1;
          } else {
            end = i + 1;
          }
          break;
        }
      default:
        if (isalnum_(str[i + 1]) || str[i + 1] == '_') {
          size_t j = i + 1;
          shvar *v;
          while (j < slen && (isalnum_(str[j]) || str[j] == '_'))
            j++;
          v = findvar_n(str + i + 1, j - (i + 1));
          if (v) {
            val = shvar_val(v);
            vlen = strlen(val);
          }
          end = j;
        } else {
          end = i + 1;
        }
    }
    if (val)
      st_write(val, vlen, tlen);
    i = end;
  }
  *outlen = tlen;
  return grab_str(tlen);
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
  static char f[4096], vcpy[256];

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
        for (s = val; *s; s++) {
          if (flen >= sizeof(f) - 1) {
            nts(f, sizeof(f) - 1);
            goto done;
          }
          f[flen++] = *s;
        }
      }
      i += varlen;
    } else {
      if (flen >= sizeof(f) - 1) {
        nts(f, sizeof(f) - 1);
        goto done;
      }
      f[flen++] = p[i++];
    }
  }
  f[flen] = '\0';
done:
  s = st_alloc(flen + 1);
  memcpy(s, f, flen + 1);
  return s;
}

/** expand argument vector */
char **
expand_argv(wf **args, size_t *restrict t)
{
  size_t cap, fargc, tlen;
  size_t i, argc, elen;
  wf **fargv;
  char **argv;
  // enum {  };

  *t = 0;
  // cap = 0;
  cap = 64;
  fargc = 0;
  argc = 0;
  fargv = NULL;

  while (args[argc])
    argc++;
  // for (size_t i = 0; i < argc; i++)
  //   cap += args[i]->len;
  argv = st_alloc(cap * sizeof(char *));

  for (i = 0; i < argc; i++) {
    wf *expanded;
    if (!(expanded = exp_word(args[i], &elen)))
      return NULL;
    fargv = splitword(expanded, &tlen);
    *t += tlen;
    for (int j = 0; fargv[j]; j++) {
      if (!fflag) {
        int glob = 0;
        for (wf *f = fargv[j]; f; f = f->next)
          if (f->qs == QNONE && f->word &&
              !(f->word[0] == '[' && f->word[1] == '\0') &&
              ismetachar(f->word, f->len)) {
            glob = 1;
            break;
          }
        if (glob) {
          char *flat = join_wf(fargv[j]);
          char **match = NULL;
          int mc = globexpand(flat, &match);
          if (mc > 0) {
            for (int k = 0; k < mc; k++) {
              if (fargc >= cap) {
                cap *= 2;
                char **new = st_alloc(cap * sizeof(char *));
                memcpy(new, argv, fargc * sizeof(char *));
                argv = new;
              }
              argv[fargc++] = match[k];
            }
          } else
            argv[fargc++] = flat;
        } else {
          argv[fargc++] = join_wf(fargv[j]);
        }
      } else
        argv[fargc++] = join_wf(fargv[j]);
    }
  }
  argv[fargc] = NULL;
  return argv;
}

/** expand word with variable substitution */
__attribute__((hot)) wf *
exp_word(wf *wordf, size_t * restrict rlen)
{
  size_t len = 0;
  size_t i;
  wf *f, *head = NULL, *tail = NULL;
  char buf[16];

  for (f = wordf; f; f = f->next) {
    char *val = NULL;
    size_t vlen = 0;
    shvar *v;
    switch (f->qs) {
      case QDOUBLE:
      case QSINGLE:
        append_wf(&head, &tail, f->word, f->len, f->qs);
        len += f->len;
        break;
      case QNONE:
        i = 0;
        char *expanded;
        while (i < f->len) {
          size_t end = 0;
          if (f->word[i] == '~') {
            size_t elen = 0;
            end = 0, 
            expanded = exp_tilde(f->word, i, &end, &elen);
            if (expanded) {
              append_wf(&head, &tail, expanded, elen, f->qs);
              len += elen;
              i = end;
            } else {
              size_t r = i;
              i++;
              while (i < f->len && f->word[i] != '~')
                i++;
              append_wf(&head, &tail, f->word + r, i - r, f->qs);
              len += i - r;
            }
          } else {
            size_t r = i;
            i++;
            while (i < f->len && f->word[i] != '~')
              i++;
            append_wf(&head, &tail, f->word + r, i - r, f->qs);
            len += i - r;
          }
        }
        break;
      case QARITH:
        {
          long long val;
          char buf[32], *prsts;
          size_t rlen;
          stmark arithm;
          arithm = stack_mark();
          val = arith_eval(f->word, f->len);
          stack_restore(arithm);
          rlen = lltoa(val, buf);
          prsts = st_strndup(buf, rlen);
          append_wf(&head, &tail, prsts, rlen, QARITH);
          len += rlen;
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
          cmdsub = parse_list(TEOF);

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
          free_tree(cmdsdup);
          append_wf(&head, &tail, cmdsubpos, sublen, f->qs);
          len += sublen;
          break;
        }
        /* ${:} parameter expansion
         * - sets default value if unset or null
         * = if unset or null we set variable to value in var_tab
         * ? error and exit if unset or null
         * + if value is set substitute
         */
      case QBRACE:
      case QBRACE_DQ:
        if (f->word[0] == '#') {
          size_t rlen;
          if ((v = findvar_n(f->word + 1, f->len - 1))) {
            rlen = strlen(shvar_val(v));
            vlen = lltoa(rlen, buf);
          } else {
            vlen = 1;
            buf[0] = '0';
            buf[1] = '\0';
          }
          val = st_strndup(buf, vlen);
          goto append;
        }
        size_t op = 0;
        for (size_t j = 0; j < f->len; j++) {
          if (f->word[j] == ':') {
            char c = f->word[j + 1];
            if (c == '-' || c == '=' || c == '?' || c == '+') {
              op = j + 1;
              break;
            }
          }
        }
        if (op > 0) {
          int isnull;
          v = findvar_n(f->word, op - 1);
          if (v)
            val = shvar_val(v);
          else
            val = NULL;
          isnull = (val == NULL || *val == '\0');

          switch (f->word[op]) {
            char *name;
            case '-':
              if (isnull) {
                val = exp_str(f->word + op + 1, f->len - op - 1, &vlen);
              }
              break;
            case '=':
              if (isnull) {
                val = exp_str(f->word + op + 1, f->len - op - 1, &vlen);
                name = st_strndup(f->word, op - 1);
                setvar(name, val, 0);
              }
              break;
            case '?':
              if (isnull) {
                name = st_strndup(f->word, op - 1);
                shwarnx(name, f->word + op + 1);
                if (!iflag)
                  exit(1);
                return NULL;
              }
              break;
            case '+':
              if (!isnull) {
                val = exp_str(f->word + op + 1, f->len - op - 1, &vlen);
              } else {
                val = NULL;
                vlen = 0;
              }
              break;
          }
          // vlen = v->flen - (v->nlen + 2);
          if (!vlen && val)
            vlen = strlen(val);
          goto append;
        }
      /* falls through */
      case QVAR:
      case QVAR_DQ:
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
            case '#':
              vlen = lltoa(sh_argc, buf);
              val = st_strndup(buf, vlen);
              break;
          }
        }

        if (is_posparam(f->word, f->len)) {
          int n = 0;
          for (size_t i = 0; i < f->len; i++)
            n = n * 10 + (f->word[i] - '0');
          val = get_posparam(n);
          vlen = val ? strlen(val) : 0;
          goto append;
        }
        v = findvar_n(f->word, f->len);
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
append:
        if (val) {
          int qs = (f->qs == QVAR || f->qs == QBRACE) ? QNONE : f->qs;
          append_wf(&head, &tail, val, vlen, qs);
          len += vlen;
        }
        break;

      case QHEREDOC:
        {
          size_t elen;
          char *exp = exp_str(f->word, f->len, &elen);
          append_wf(&head, &tail, exp, elen, QHEREDOC);
          len += elen;
        }
        break;
    }
  }
  if (rlen)
    *rlen = len;
  return head;
}

static wf **
splitword(wf *f, size_t * restrict tlen)
{
  enum {
    M_IMMUNE,
    M_IFSWS,
    M_IFSN,
    M_NORMAL
  };
  wf **fargv;
  wf *chead, *ctail, *cf;
  size_t fpos, cap, fargc;

  if (tlen)
    *tlen = 0;

  if (ifsnull || !f) {
    fargv = st_alloc(2 * sizeof(wf *));
    fargv[0] = f;
    fargv[1] = NULL;
    return fargv;
  }

  cap = 16;
  fargv = st_alloc(cap * sizeof(wf *));
  fargc = 0;
  chead = NULL;
  ctail = NULL;
  cf = f;
  fpos = 0;
  // field = NULL;

  while (cf) {
    size_t s = fpos;

    // TODO: add in simd
    while (fpos < cf->len) {
      int insect = (cf->qs == QNONE || cf->qs == QCMDSUB);
      char c = cf->word[fpos];
      int mode;

      if (!insect)
        mode = M_IMMUNE;
      else if (is_ws(c))
        mode = M_IFSWS;
      else if (ifschar[(unsigned char)c])
        mode = M_IFSN;
      else
        mode = M_NORMAL;

      switch (mode) {
        case M_IMMUNE:
          fpos++;
          continue;
        case M_IFSWS:
          if (fpos > s) {
            append_wf(&chead, &ctail, cf->word + s, fpos - s, cf->qs);
          }
          if (chead) {
            if (fargc >= cap) {
              cap *= 2;
              wf **new = st_alloc(cap * sizeof(wf *));
              memcpy(new, fargv, fargc * sizeof(wf *));
              fargv = new;
            }
            append_wf(&chead, &ctail, NULL, 0, QNONE);
            fargv[fargc++] = chead;
            chead = NULL;
            ctail = NULL;
          }
          fpos++;
          s = fpos;
          continue;
        case M_IFSN:
          if (fpos > s)
            append_wf(&chead, &ctail, cf->word + s, fpos - s, cf->qs);
          append_wf(&chead, &ctail, NULL, 0, QNONE);
          if (fargc >= cap) {
            cap *= 2;
            wf **new = st_alloc(cap * sizeof(wf *));
            memcpy(new, fargv, fargc * sizeof(wf *));
            fargv = new;
          }
          fargv[fargc++] = chead;
          chead = NULL;
          ctail = NULL;
          fpos++;
          s = fpos;
          continue;
        case M_NORMAL:
          fpos++;
          continue;
      }
    }
    if (cf->qs != QNONE && cf->qs != QCMDSUB)
      append_wf(&chead, &ctail, cf->word, cf->len, cf->qs);
    else if (fpos > s)
      append_wf(&chead, &ctail, cf->word + s, fpos - s, cf->qs);
    cf = cf->next;
    fpos = 0;
  }

  if (chead) {
    append_wf(&chead, &ctail, NULL, 0, QNONE);
    if (fargc >= cap) {
      cap *= 2;
      wf **new = st_alloc(cap * sizeof(wf *));
      memcpy(new, fargv, fargc * sizeof(wf *));
      fargv = new;
    }
    fargv[fargc++] = chead;
  }

  fargv[fargc] = NULL;
  return fargv;
}
