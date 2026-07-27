/* expand.c - variable/string expandsion logic */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "arith.h"
#include "env.h"
#include "errmsg.h"
#include "exec.h"
#include "expand.h"
#include "glob.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "opts.h"
#include "parse.h"
#include "utils.h"
#include "var.h"

char ifschar[256];
#define _INCHLD (1 << 0)

static int run_cmdsub(const cmd_tree *);
static char **splitnglob(wf *restrict, size_t *restrict);
int incmdsub = 0;

/** get the pid shell variable */
#define varpid() (st_strdup(gvar.pid_s))
#define varbgpid() (st_strdup(gvar.bgpid_s))
#define varargc() \
  char *buf; \
  buf = st_alloc(16); \
  *olen = lltoa(SHARGC, buf); \
  return buf;

#define chk_cap(arc, c, arv, t) \
  if ((arc) >= (c)) { \
    (c) *= 2; \
    streallocar(arv, c, arc, t); \
  }

static inline char *
vardash(size_t *o)
{
  char *buf = st_alloc(32);
  int p = 0;
  for (int i = 0; i < SHOPTC; i++) {
    if (GETSHOPT(i))
      buf[p++] = shoptch[i];
  }
  buf[p] = '\0', *o = p;
  return buf;
}

/** get the variable for the return status of last command */
static inline char *
varstatus(size_t *o)
{
  size_t n, i, e;
  char *buf, t;

  i = 0;
  n = LSTATUS;
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
    return SHARGV0 ? SHARGV0 : "";

  if (n < 0 || n > SHARGC)
    return "";
  return SHARGV[n - 1] ? SHARGV[n - 1] : "";
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

int
run_cmdsub(const cmd_tree *restrict n)
{
  int wstatus, ret, pipefd[2];
  pid_t pid;
  static char buf[MINSTACK_S];
  size_t len;

  if (!n)
    return -1;
  pipefd[0] = pipefd[1] = -1;
  if (pipe(pipefd) < 0) {
    sherrx(1, "create pipe");
    ret = -1;
    goto cleanup;
  }

  pid = fork();
  switch (pid) {
    case -1:
      warn("fork");
      ret = -1;
      goto cleanup;
    case 0:
      if (close(pipefd[0]) < 0)
        perror("close");
      if (dup2(pipefd[1], STDOUT_FILENO) < 0)
        warn("dup");
      if (close(pipefd[1]) < 0)
        warn("close");
      cleartraps();
      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);
      if (mflag) {
        signal(SIGTSTP, SIG_DFL);
        signal(SIGTTIN, SIG_DFL);
        signal(SIGTTOU, SIG_DFL);
      }
      incmdsub = 1;
      LSTATUS = run_commands(n, _INCHLD);
      incmdsub = 0;
      fflush(NULL);
      _exit(LSTATUS);
    default:
      {
        int n;
        len = 0;

        if (close(pipefd[1]) < 0) {
          sherrx(1, "close pipe");
          ret = -1;
          goto cleanup;
        }

        char *tmp = NULL;
        size_t tmpcap = 0, tmplen = 0;

        while ((n = read(pipefd[0], buf, sizeof(buf)))) {
          if (n > 0) {
            if (tmplen + n > tmpcap) {
              size_t newcap = tmpcap ? tmpcap * 2 : 65536;
              while (newcap < tmplen + n)
                newcap *= 2;
              char *new = salloc(newcap);
              if (tmp)
                memcpy(new, tmp, tmplen);
              sfree(tmp);
              tmp = new;
              tmpcap = newcap;
            }
            memcpy(tmp + tmplen, buf, n);
            tmplen += n;
            continue;
          }
          if (n == 0)
            break;
          if (n == -1 && errno == EINTR)
            continue;
          if (n == -1 && errno != EINTR) {
            sfree(tmp);
            ret = -1;
            goto cleanup;
          }
        }

        while (tmplen > 0 && tmp[tmplen - 1] == '\n')
          tmplen--;
        if (stleft <= tmplen + 1)
          grow_stack(tmplen + 1);
        if (tmp)
          memcpy(stnext, tmp, tmplen);
        stnext[tmplen] = '\0';
        len = tmplen;
        stnext += tmplen + 1;
        stleft -= tmplen + 1;
        sfree(tmp);
        ret = len;
        if (waitpid(pid, &wstatus, 0) > 0)
          LSTATUS = WIFEXITED(wstatus) ? WEXITSTATUS(wstatus) : 1;
        else
          LSTATUS = 1;
      }
  }

cleanup:
  if (pipefd[0] >= 0)
    close(pipefd[0]);
  if (pipefd[1] >= 0)
    close(pipefd[1]);
  return ret;
}

static char **
splitnglob(wf *f, size_t * restrict tlen)
{
  enum {
    M_IMMUNE,
    M_IFSWS,
    M_IFSN,
    M_NORMAL
  };

  size_t argvlen = 0, argc = 0, cap = 16;
  size_t bpos, fpos;
  size_t ttl = 0;
  wf *cf;
  char **out, *buf;
  int hasglob, empty = 0, glob = 0;

  for (wf *w = f; w; w = w->next)
    argvlen += w->len;
  out = st_alloc(cap * sizeof(char *));

  if (gvar.ifsnull || !f) {
    int mc;
    size_t pos = 0;
    if (!f) {
      out[0] = NULL;
      *tlen = 0;
      return out;
    }
    buf = st_alloc(argvlen + 1);
    for (wf *w = f; w; w = w->next) {
      memcpy(buf + pos, w->word, w->len);
      pos += w->len;
    }
    buf[pos] = '\0';
    if (!fflag) {
      char **match;
      for (wf *w = f; w; w = w->next) {
        if (w->qs == QNONE && w->word &&
            !(w->word[0] == '[' && w->word[1] == '\0') &&
            ismetachar(w->word, w->len)) {
          glob = 1;
          break;
        }
      }
      if (glob) {
        mc = globexpand(buf, &match);
        for (int k = 0; k < mc; k++) {
          chk_cap(argc, cap, out, char *);
          out[argc++] = match[k];
        }
        *tlen = pos;
        out[mc] = NULL;
        return out;
      }
    }
    *tlen = pos;
    out[0] = buf;
    out[1] = NULL;
    return out;
  }

  buf = st_alloc(argvlen + 1);
  bpos = 0, hasglob = 0;
  cf = f;
  fpos = 0;

  while (cf) {
    size_t sz = fpos;
    if (cf->qs != QNONE && cf->qs != QCMDSUB) {
      if (cf->len == 0 && !bpos)
        empty = 1;
      else {
        memcpy(buf + bpos, cf->word, cf->len);
        bpos += cf->len;
      }
      cf = cf->next;
      fpos = 0;
      continue;
    }

    if (cf->qs == QNONE || cf->qs == QCMDSUB) {
      size_t clen, end;
      end = sscndelim(cf->word + sz, cf->len - sz, gvar.ifsv, gvar.ifsvlen);
      if (end >= cf->len - sz) {
        size_t csz = cf->len - sz;
        memcpy(buf + bpos, cf->word + sz, csz);
        if (!hasglob && cf->qs == QNONE && (csz != 1 || cf->word[sz] != '['))
          hasglob = ismetachar(cf->word + sz, csz);
        bpos += csz;
        cf = cf->next;
        fpos = 0;
        continue;
      }
      fpos = sz + end;
      clen = end;
      memcpy(buf + bpos, cf->word + sz, clen);
      if (!hasglob && cf->qs == QNONE && (clen != 1 || cf->word[sz] != '['))
        hasglob = ismetachar(cf->word + sz, clen);
      bpos += clen;
    }
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
          buf[bpos++] = c;
          fpos++;
          continue;
        case M_IFSWS:
          if (bpos > 0) {
            buf[bpos] = '\0';
            if (hasglob && !fflag) {
              char **match = NULL;
              int mc = globexpand(buf, &match);
              if (mc > 0) {
                if (argc + mc > cap) {
                  while (argc + mc > cap)
                    cap *= 2;
                  streallocar(out, cap, argc, char *);
                }
                for (int k = 0; k < mc; k++)
                  out[argc++] = match[k];
              } else {
                chk_cap(argc, cap, out, char *);
                out[argc++] = st_strndup(buf, bpos);
              }
            } else {
              chk_cap(argc, cap, out, char *);
              out[argc++] = st_strndup(buf, bpos);
            }
            ttl += bpos;
            bpos = 0;
            hasglob = 0;
          }
          fpos++;
          sz = fpos;
          continue;
        case M_IFSN:
          buf[bpos] = '\0';
          if (hasglob && !fflag) {
            char **match = NULL;
            int mc = globexpand(buf, &match);
            if (mc > 0) {
              if (argc + mc > cap) {
                while (argc + mc > cap)
                  cap *= 2;
                streallocar(out, cap, argc, char *);
              }
              for (int k = 0; k < mc; k++)
                out[argc++] = match[k];
            } else {
              chk_cap(argc, cap, out, char *);
              out[argc++] = st_strndup(buf, bpos);
            }
          } else {
            chk_cap(argc, cap, out, char *);
            out[argc++] = st_strndup(buf, bpos);
          }
          ttl += bpos;
          bpos = 0;
          hasglob = 0;
          fpos++;
          sz = fpos;
          continue;
        case M_NORMAL:
          buf[bpos++] = c;
          if (!hasglob && (c == '*' || c == '?' || c == '[' ))
            hasglob = 1;
          fpos++;
          continue;
        default:
          break;
      }
    }
    if (!bpos && !cf->len && !(cf->qs == QNONE || cf->qs == QCMDSUB))
      empty = 1;
    cf = cf->next;
    fpos = 0;
  }
  if (bpos > 0) {
    buf[bpos] = '\0';
    if (hasglob && !fflag) {
      char **match = NULL;
      int mc = globexpand(buf, &match);
      if (mc > 0) {
        if (argc + mc > cap) {
          while (argc + mc > cap)
            cap *= 2;
          streallocar(out, cap, argc, char *);
        }
        for (int k = 0; k < mc; k++)
          out[argc++] = match[k];
      } else {
        chk_cap(argc, cap, out, char *);
        out[argc++] = st_strndup(buf, bpos);
      }
    } else {
      chk_cap(argc, cap, out, char *);
      out[argc++] = st_strndup(buf, bpos);
    }
    ttl += bpos;
  } else if (empty) {
    chk_cap(argc, cap, out, char *);
    out[argc++] = st_strndup("", 0);
    ttl += 0;
  }
  if (tlen)
    *tlen = ttl;
  out[argc] = NULL;
  return out;
}

/** expand word with variable substitution */
__attribute__((hot)) wf *
exp_word(wf *wordf, size_t * restrict rlen)
{
  size_t end = 0, len = 0;
  size_t i;
  wf *f, *head = NULL, *tail = NULL;
  char *expanded, buf[16];

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
        while (i < f->len) {
          size_t off, tpos, elen;
          off = memchr_((f->word + i), (f->len - i), '~');
          if (off >= f->len - i) {
            append_wf(&head, &tail, (f->word + i), (f->len - i), f->qs);
            len += f->len - i;
            break;
          }
          if (off > 0) {
            append_wf(&head, &tail, (f->word + i), off, f->qs);
            len += off;
            i += off;
          }
          tpos = i;
          elen = 0;
          expanded = exp_tilde(f->word, tpos, &end, &elen);
            if (expanded) {
              append_wf(&head, &tail, expanded, elen, f->qs);
              len += elen;
              i = end;
            } else {
              append_wf(&head, &tail, f->word + tpos, 1, f->qs);
              len++;
              i = tpos + 1;
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
          stmark csmark;

          savesl = stleft;
          cmdsubpos = stnext;
          csmark = stack_mark();
          setinputstrn(f->word, f->len);
          notclosed = 0;
          cmdsub = parse_list(TEOF);

          popinput();
          cmdsdup = tree_dup(cmdsub);
          stack_restore(csmark);
          if (!cmdsdup)
            return NULL;
          stnext = cmdsubpos;
          stleft = savesl;
          if ((sublen = run_cmdsub(cmdsdup)) < 0) {
            free_tree(cmdsdup);
            return NULL;
          }
          cmdsubpos = current->buf + (cmdsubpos - csmark.current->buf);
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
        size_t op = 0, nlen = 0;
        for (size_t j = 0; j < f->len; j++) {
          if (f->word[j] == ':') {
            char c = f->word[j + 1];
            if (c == '-' || c == '=' || c == '?' || c == '+') {
              op = j + 1, nlen = op - 1;
              break;
            }
          } else if ((f->word[j] == '#' || f->word[j] == '%') && j > 0) {
            op = nlen = j;
            break;
          }
        }
        if (op > 0) {
          int isnull;
          v = findvar_n(f->word, nlen);
          if (v) {
            val = shvar_val(v);
            vlen = vallen(v);
          } else {
            val = NULL;
          }
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
                shwarn(name, f->word + op + 1);
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
            case '#':
              {
                if (!val)
                  break;
                int lrg = 0, pstrt, plen;
                size_t explen;
                char *expat, *vcpy;

                lrg = (f->word[op + 1] == '#');
                pstrt = op + 1 + lrg;
                plen = f->len - pstrt;
                expat = exp_str(f->word + pstrt, plen, &explen);
                vcpy = st_strndup(val, vlen);
                if (!vlen || !explen)
                  break;

                if (lrg) {
                  for (size_t n = vlen; n > 0; n--) {
                    char sv = vcpy[n];
                    vcpy[n] = '\0';
                    if (globmatch(expat, vcpy, 0)) {
                      vcpy[n] = sv;
                      val = st_strndup(vcpy + n, vlen - n);
                      vlen -= n;
                      break;
                    }
                    vcpy[n] = sv;
                  }
                } else {
                  for (size_t n = 1; n <= vlen; n++) {
                    char sv = vcpy[n];
                    vcpy[n] = '\0';
                    if (globmatch(expat, vcpy, 0)) {
                      vcpy[n] = sv;
                      val = st_strndup(vcpy + n, vlen - n);
                      vlen -= n;
                      break;
                    }
                    vcpy[n] = sv;
                  }
                }
              }
              break;
            case '%':
              {
                if (!val)
                  break;
                int lrg = 0, pstrt, plen;
                size_t explen;
                char *expat, *vcpy;

                lrg = (f->word[op + 1] == '%');
                pstrt = op + 1 + lrg;
                plen = f->len - pstrt;
                expat = exp_str(f->word + pstrt, plen, &explen);
                vcpy = st_strndup(val, vlen);

                if (!vlen || !explen)
                  break;
                if (lrg) {
                  for (size_t n = vlen; n > 0; n--) {
                    if (globmatch(expat, vcpy + vlen - n, 0)) {
                      val = st_strndup(vcpy, vlen - n);
                      vlen -= n;
                      break;
                    }
                  }
                } else {
                  for (size_t n = 1; n <= vlen; n++) {
                    if (globmatch(expat, vcpy + vlen - n, 0)) {
                      val = st_strndup(vcpy, vlen - n);
                      vlen -= n;
                      break;
                    }
                  }
                }
              }
              break;
            default:
              break;
          }
          if (!vlen && val && v)
            vlen = vallen(v);
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
            case '-':
              val = vardash(&vlen);
              break;
            case '#':
              vlen = lltoa(SHARGC, buf);
              val = st_strndup(buf, vlen);
              break;
            default:
              break;
          }
        }

        if (is_posparam(f->word, f->len)) {
          size_t n = 0;
          for (size_t i = 0; i < f->len; i++)
            n = n * 10 + (f->word[i] - '0');
          val = get_posparam(n);
          vlen = val ? strlen(val) : 0;
          goto append;
        }
        v = findvar_n(f->word, f->len);
        if (v) {
          val = shvar_val(v);
          vlen = vallen(v);
        } else if (uflag && f->len > 0) {
          char name[64];
          nmemcpy(name, f->word, f->len);
          UFLAGMSG(name);
          gstate.nounseterr = 1;
        }
append:
        if (val) {
          int qs = (f->qs == QVAR || f->qs == QBRACE) ? QNONE : f->qs;
          append_wf(&head, &tail, val, vlen, qs);
          len += vlen;
        } else {
          int qs = (f->qs == QVAR || f->qs == QBRACE) ? QNONE : f->qs;
          append_wf(&head, &tail, "", 0, qs);
          len += val ? vlen : 0;
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

/** expand argument vector */
char **
expand_argv(wf **args, size_t *restrict t)
{
  size_t i, elen, tlen;
  char **argv;
  size_t cap = 64;
  size_t fargc = 0;
  *t = 0;

  argv = (char **)st_alloc(cap * sizeof(char *));
  for (i = 0; args[i]; i++) {
    wf *w = args[i];
    if (!w->next && w->qs == QNONE &&
        sscndelim(w->word, w->len, "$`\\~*?[", 7) >= w->len) {
      chk_cap(fargc, cap, argv, char *);
      argv[fargc++] = w->word;
      *t += w->len;
      continue;
    }
    if (SHARGC && w->len == 1 && (w->word[0] == '@' || w->word[0] == '*') &&
        (w->qs & (QVAR | QVAR_DQ | QBRACE | QBRACE_DQ))) {
      if (w->word[0] == '@' && (w->qs & (QVAR_DQ | QBRACE_DQ))) {
        for (int j = 1; j <= SHARGC; j++) {
          chk_cap(fargc, cap, argv, char *);
          argv[fargc++] = st_strdup(get_posparam(j));
        }
        continue;
      }
      shvar *ifs;
      int ifsc = 0;
      if ((ifs = findvar_n("IFS", 3))) {
        if (ifs->var)
          ifsc = (int)*shvar_val(ifs);
      } else {
        ifsc = ' ';
      }
      size_t pos = 0;
      size_t lenarr[SHARGC + 1];
      tlen = 0;
      memset(lenarr, 0, sizeof(lenarr));
      for (int j = 1; j <= SHARGC; j++) {
        char *p = get_posparam(j);
        tlen += lenarr[j] = p ? strlen(p) : 0;
        if (j < SHARGC && ifsc)
          tlen++;
      }
      char *buf = st_alloc(tlen + 1);
      for (int j = 1; j <= SHARGC; j++) {
        char *p = get_posparam(j);
        if (p) {
          memcpy(buf + pos, p, lenarr[j]);
          pos += lenarr[j];
        }
        if (j < SHARGC && ifsc)
          buf[pos++] = ifsc;
      }
      buf[pos] = '\0';
      chk_cap(fargc, cap, argv, char *);
      if (w->qs & (QVAR | QBRACE)) {
        argv[fargc++] = st_strndup(buf, tlen);
        continue;
      }
      wf one;
      char **fields;
      one.qs = QNONE;
      one.word = buf;
      one.len = tlen;
      one.flags = 0;
      one.next = NULL;
      fields = splitnglob(&one, &tlen);
      *t += tlen;
      for (int j = 0; fields[j]; j++) {
        chk_cap(fargc, cap, argv, char *);
        argv[fargc++] = fields[j];
      }
      continue;
    }

    wf *expanded;
    if (!(expanded = exp_word(args[i], &elen)))
      return NULL;
    char **fields;
    fields = splitnglob(expanded, &tlen);
    *t += tlen;
    for (int j = 0; fields[j]; j++) {
      chk_cap(fargc, cap, argv, char *);
      argv[fargc++] = fields[j];
    }
  }
  argv[fargc] = NULL;
  return argv;
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
exp_str(char *restrict str, size_t slen, size_t *restrict outlen)
{
  size_t i = 0, vlen, end, tlen = 0;
  char buf[32], *val;

  while (i < slen) {
    size_t s = i;
    while (i < slen && str[i] != '$')
      i++;
    if (i > s)
      st_write(str + s, i - s, tlen);
    if (i >= slen)
      break;
    vlen = 0;
    val = NULL;
    switch (str[i + 1]) {
      case '$':
        val = gvar.pid_s;
        vlen = strlen(val);
        end = i + 2;
        break;
      case '?':
        {
          char _buf[24];
          vlen = lltoa(LSTATUS, _buf);
          val = _buf;
          end = i + 2;
        }
        break;
      case '!':
        val = gvar.bgpid_s;
        vlen = strlen(val);
        end = i + 2;
        break;
      case '#':
        vlen = lltoa(SHARGC, buf);
        val = buf;
        end = i + 2;
        break;
      case '-':
        val = vardash(&vlen);
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
                vlen = vallen(v);
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
            vlen = vallen(v);
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

  hm = gvar.home;
  if (word[s] != '~')
    return NULL;
  if (s == 0 && (word[s + 1] == '\0' || word[s + 1] == '/')) {
    if (!hm)
      return NULL;
    *e = s + 1;
    *olen = gvar.homelen;
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
      *olen = gvar.homelen;
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
          f[flen++] = LSTATUS == 0 ? '$' : 'X';
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
          case '-':
            val = vardash(&vlen);
            break;
          case '#':
            {
              char buf[16];
              vlen = lltoa(SHARGC, buf);
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
                vlen = vallen(v);
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

