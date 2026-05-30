/* expand.c - variable/string expandsion logic */
#include "exec.h"
#include <stddef.h>
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <limits.h>
#include <stdio.h>

#include "expand.h"
#include "lex.h"
#include "main.h"
#include "malloc.h"
#include "opts.h"
#include "parse.h"
#include "var.h"

static char *var_n(const char *, size_t, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
static char *lookupvar(const char *, size_t);

/** get the pid shell variable */
#define varpid() (st_strdup(sh_pid_s))
#define varargc() \
  char *buf; \
  buf = st_alloc(16); \
  snprintf(buf, 16, "%d", sh_argc); \
  return buf;

/** get the variable for the return status of last command */
static inline char *
varstatus(void)
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

/** expand $var style variables */
char *
var_n(const char *args, size_t i, size_t *end)
{
  const char *env_var;
  char *var;
  size_t j, vt_l;

  for (j = i + 1;; j++) {
    if ((isalnum_(args[j]) == 0 && args[j] != '_') || args[j] == '\0') {
      vt_l = j - (i + 1);

      if (vt_l == 0) {
        env_var = NULL;
        break;
      } else {
        env_var = lookupvar(&args[i + 1], vt_l);
        break;
      }
    }
  }

  *end = j;
  if (!env_var) {
    if (uflag && vt_l > 0) {
      char name[64];
      // WARN: this might buffer overflow at some point, need to fix that later probably
      nmemcpy(name, &args[i+1], vt_l);
      UFLAGMSG(name);
      nounseterr = 1;
    }
      return NULL;
  }
  var = st_strdup(env_var);
  return var;
}

/** expand ${var} style variables */
char *
varbrace_n(const char *args, size_t i, size_t *end)
{
  char *var, *env_var;
  size_t j, vt_l;

  if (strchr(args, '}') == NULL)
    return NULL;

  for (j = i + 1;; j++) {
    if (args[j] == '}') {
      vt_l = j - (i + 2);
      if (vt_l == 0)
        return NULL;
      else {
        env_var = lookupvar(&args[i + 2], vt_l);
        break;
      }
    }
  }

  *end = j;
  if (!env_var) {
    if (uflag && vt_l > 0) {
      char name[64];
      nmemcpy(name, &args[i+2], vt_l);
      UFLAGMSG(name);
      nounseterr = 1;
    }
      return NULL;
  }
  var = st_strdup(env_var);
  return var;
}

/** lookup variable by name */
char *
lookupvar(const char *vt, size_t vlen)
{
  char *var;
  shvar *v;
  int n;
  size_t i;

  n = 0;
  if (is_posparam(vt, vlen)) {
    for (size_t j = 0;j < vlen; j++)
      n = n * 10 + (vt[j] - '0');
    var = get_posparam(n);
  } else {
    i = hash_n(vt, vlen, VAR_BUCKETS);
    v = var_tab[i];
    while (v) {
      if (memcmp(v->var, vt, vlen) == 0)
        break;
      v = v->next;
    }
    if (v)
      var = shvar_val(v);
    else
      var = NULL;
  }

  return var;
}

/** expand variables in word */
char *
exp_var(char *word, size_t s, size_t *e)
{
  if (word[s] != '$')
    return NULL;

  if (word[s + 1] == '$') {
    *e = s + 2;
    return varpid();
  } else if (word[s + 1] == '?') {
    *e = s + 2;
    return varstatus();
  } else if (word[s + 1] == '#') {
    *e = s + 2;
    varargc();
  } else if (word[s + 1] == '{') {
    return varbrace_n(word, s, e);
  } else {
    return var_n(word, s, e);
  }
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
  // XXX: double check this
  fclose(pw);
  return NULL;
}

char *
exp_tilde(char *word, size_t s, size_t *e)
{
  char *hm, strt;
  size_t end;

  if (word[s] != '~')
    return NULL;
  if (s == 0 && (word[s + 1] == '\0' || word[s + 1] == '/')) {
    if (!(hm = getvar("HOME")))
      return NULL;
    *e = s + 1;
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
      return hm;
    }
  }
  return NULL;
}

/** take in ps1 char * to do variable expansion for prompt */
char *
expand_ps1(char *p)
{
  size_t i, end, varlen, cbrace, flen;
  char *s, *expanded;
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
        while (p[cbrace] && p[cbrace] == '}') {
          cbrace++;
          if (!p[cbrace]) {
            f[flen++] = p[i++];
            continue;
          }
          varlen = cbrace - i + 1;
          memcpy(vcpy, p + i, varlen);
          vcpy[varlen] = '\0';
        }
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
      if ((expanded = exp_var(vcpy, 0, &end))) {
        for (s = expanded; *s; s++)
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
expand_argv(wf **args, size_t *t)
{
  size_t i, argc = 0;
  while (args[argc])
    argc++;

  *t = 0;
  char **argv = st_alloc((argc + 1) * sizeof(char *));
  for (i = 0; i < argc; i++) {
    argv[i] = exp_word(args[i]);
    *t += args[i]->len;
    if (!argv[i])
      return NULL;
  }
  argv[argc] = NULL;
  return argv;
}

/** expand word with variable substitution */
__attribute__((hot)) char *
exp_word(wf *wordf)
{
  size_t end, i;
  size_t len;
  wf *f;
  // char *s;
  char *expanded;

  len = 0;
  for (f = wordf; f; f = f->next) {
    switch (f->qs) {
      case QSINGLE:
        if (f->len >= stleft)
          grow_stack(f->len);
        memcpy(stnext, f->word, f->len);
        stnext += f->len;
        stleft -= f->len;
        len += f->len;
        break;
      case QDOUBLE:
      case QNONE:
        i = 0;
        while (i < f->len) {
          size_t s, r;
          s = i;
          if (f->qs == QNONE) {
            while (i < f->len && f->word[i] != '$' && f->word[i] != '~')
              i++;
          } else {
            while (i < f->len && f->word[i] != '$')
              i++;
          }
          r = i - s;
          if (r) {
            if (r >= stleft)
              grow_stack(r);
            memcpy(stnext, f->word + s, r);
            stnext += r;
            stleft -= r;
            len += r;
          }
          if (i>= f->len)
            continue;
          if (f->word[i] == '~' && f->qs == QNONE) {
            expanded = exp_tilde(f->word, i, &end);
            if (expanded) {
              size_t elen;
              elen = strlen(expanded);
              if (elen >= stleft)
                grow_stack(elen);
              memcpy(stnext, expanded, elen);
              stnext += elen;
              stleft -= elen;
              len += elen;
              i = end;
            } else {
              st_putc('~');
              len++;
              i++;
            }
          } else if (f->word[i] != '$') {
            st_putc(f->word[i]);
            len++;
            i++;
          } else {
            expanded = exp_var(f->word, i, &end);
            if (expanded) {
              size_t vlen;
              vlen = strlen(expanded);
              if (vlen >= stleft)
                grow_stack(vlen);
              memcpy(stnext, expanded, vlen);
              stnext += vlen;
              stleft -= vlen;
              len += vlen;
              i = end;
            } else {
              i = end;
            }
          }
        }
        break;
      case QCMDSUB:
      case QCMDSUB_DQ:
        {
          cmd_tree *cmdsub;
          char *res;

          pushstring(f->word, f->len, 0);
          last_tok = SHTOK(TNONE);
          chkwd = 0;
          notclosed = 0; // XXX: I really feel like this is wrong
          cmdsub = parse_list(TEOF);
          if (!cmdsub)
            fprintf(stderr, "cmdsub was null\n");

          if ((res = run_cmdsub(cmdsub))) {
            size_t reslen;
            reslen = strlen(res);
            if (reslen >= stleft)
              grow_stack(reslen);
            memcpy(stnext, res, reslen);
            stnext += reslen;
            stleft -= reslen;
            len += reslen;
          }
          break;
        }
    }
  }

  return grab_str(len);
}
