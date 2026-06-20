#define _POSIX_C_SOURCE 200809L

#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "arg.h"
#include "env.h"
#include "exec.h"
#include "expand.h"
#include "main.h"
#include "opts.h"
#include "path.h"
#include "simd.h"
#include "utils.h"
#include "var.h"

static shvar var_tab_init[VAR_BUCKETS_INIT];
static char **env_cache;
static int env_dirty = 1;
shvar *var_tab = var_tab_init;
shvar *var_cache[VAR_CACHE_S];
size_t var_tab_size = VAR_BUCKETS_INIT;
size_t var_count;
tmp_var localvars[LOCAL_MAX];
size_t localsp;

void
resize_var_tab(void)
{
  size_t ns;
  shvar *n;

  ns = var_tab_size * 2;
  if (!(n = slcalloc(ns, sizeof(shvar))))
    return;

  for (size_t i = 0; i < var_tab_size; i++) {
    shvar *v;
    size_t h;
    v = &var_tab[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    h = hash_n(v->var, v->nlen, ns);
    while (n[h].var)
      h = (h + 1) % ns;
    n[h] = *v;
  }

  if (var_tab != var_tab_init)
    slfree(var_tab);
  var_tab = n;
  var_tab_size = ns;
  memset(var_cache, 0, sizeof(var_cache));
}

/** find variable by name */
__attribute__((hot)) shvar *
findvar_n(const char *restrict name, size_t nlen)
{
  unsigned int ci, bucket;
  shvar *v, *cv, *end;

  bucket =  hash_n(name, nlen, var_tab_size);
  ci = bucket & (VAR_CACHE_S - 1);
  cv = var_cache[ci];
  if (cv && cv->var && cv->var != TOMBSTONE && cv->nlen == nlen &&
      smemcmp(cv->var, name, nlen)) {
    return cv;
  }

  end = var_tab + var_tab_size;
  v = var_tab + bucket;
  for (;;) {
    if (!v->var)
      return NULL;
    if (v->var != TOMBSTONE && v->nlen == nlen && smemcmp(v->var, name, nlen)) {
      return v;
    }
    if (++v >= end)
      v = var_tab;
  }
}

/** set variable value */
void
setvar(const char *restrict name, char *restrict val, shvar_flags flags)
{
  if (aflag)
    flags |= VEXPRT;
  shvar *v, *n, *end;
  char *nvar;
  size_t vlen, nlen, flen, ci;

  nlen = strlen(name);
  vlen = val ? strlen(val) : 0;
  flen = nlen + vlen + 2;
  end = var_tab + var_tab_size;
  v = var_tab + hash_n(name, nlen, var_tab_size);
  n = NULL;

  for (;;) {
    if (!v->var) {
      if (!n)
        n = v;
      break;
    }
    if (v->var == TOMBSTONE) {
      if (!n)
        n = v;
    } else if (v->nlen == nlen && smemcmp(v->var, name, nlen)) {
      if (v->flags & VREADONLY)
        return;
      if (v->flen >= flen) {
        if (val)
          memcpy(v->var + nlen + 1, val, vlen + 1);
        else
          v->var[nlen + 1] = '\0';
      } else {
        if (!(nvar = slalloc(flen)))
          return;
        memcpy(nvar, name, nlen);
        nvar[nlen] = '=';
        if (val)
          memcpy(nvar + nlen + 1, val, vlen + 1);
        else
          nvar[nlen + 1] = '\0';
        slfree(v->var);
        v->var = nvar;
        v->nlen = nlen;
        v->flen = flen;
      }
      if (v->flags & VEXPRT || flags & VEXPRT)
        env_dirty = 1;
      v->flags = flags;
      goto callback;
    }
    if (++v >= end)
      v = var_tab;
  }

  if (!(nvar = slalloc(flen)))
    return;
  memcpy(nvar, name, nlen);
  nvar[nlen] = '=';
  if (val)
    memcpy(nvar + nlen + 1, val, vlen + 1);
  else
    nvar[nlen + 1] = '\0';
  if (flags & VEXPRT)
    env_dirty = 1;
  n->var = nvar;
  n->nlen = nlen;
  n->flen = flen;
  n->flags = flags;
  n->func = NULL;
  v = n;
  var_count++;
  if (var_count > var_tab_size * 7 / 10) {
    resize_var_tab();
    end = var_tab + var_tab_size;
    v = var_tab + hash_n(name, nlen, var_tab_size);
    for (;;) {
      if (v->var && v->var != TOMBSTONE && v->nlen == nlen &&
          smemcmp(v->var, name, nlen)) {
        break;
      }
      if (++v >= end)
        v = var_tab;
    }
  }

callback:
  ci = hash_n(name, nlen, VAR_CACHE_S);
  var_cache[ci] = v;
  if (v->func)
    v->func(val);
  return;
}

/** unset variable */
void
rmvar(const char *name)
{
  size_t ci;
  shvar *v, *end;
  size_t nlen;

  nlen = strlen(name);
  end = var_tab + var_tab_size;
  v = var_tab + hash_n(name, nlen, var_tab_size);

  for (;;) {
    if (!v->var)
      return;
    if (v->var != TOMBSTONE && v->nlen == nlen &&
        smemcmp(v->var, name, nlen)) {
      slfree(v->var);
      v->var = TOMBSTONE;
      v->nlen = 0;
      v->flags = 0;
      v->func = NULL;
      var_count--;
      env_dirty = 1;
      ci = hash_n(name, nlen, VAR_CACHE_S);
      if (var_cache[ci] == v)
        var_cache[ci] = NULL;
      return;
    }
    if (++v >= end)
      v = var_tab;
  }
}

tmp_var
grabvar(char *name)
{
  tmp_var tmp;
  shvar *v;

  // XXX: again where is nlen/len??
  v = findvar(name);
  if (v) {
    tmp.set = 1;
    tmp.name = st_strdup(name);
    tmp.val = st_strdup(shvar_val(v));
    tmp.oldflags = v->flags;
  } else {
    tmp.set = 0;
    tmp.name = st_strdup(name);
    tmp.val = NULL;
  }
  return tmp;
}

char **
rebuild_env(char **sh_env)
{
  static size_t tmplen[MAX_ENV], lenarr[MAX_ENV];
  size_t c, sh_c, j, len, skipc;
  static char *tmpname[MAX_ENV], *shadowed[MAX_ENV];
  char **arr, **cmd_env, **mem;

  cmd_env = NULL;
  c = 0;
  j = 0;
  len = 0;
  sh_c = 0;
  skipc = 0;

  if (sh_env) {
    array_len(sh_env, sh_c);
    for (size_t i = 0; i < sh_c; i++) {
      if (c >= MAX_ENV)
        break;
      char *eq;
      lenarr[c] = strlen(sh_env[i]) + 1;
      eq = strchrnul_(sh_env[i], '=');
      tmpname[i] = sh_env[i];
      tmplen[i] = eq - sh_env[i];
      len += lenarr[c++];
    }
  }

  for (size_t i = 0; i < var_tab_size; i++) {
    size_t namelen, skip;
    shvar *v;
    v = &var_tab[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    if (v->flags & VEXPRT) {
      if (c >= MAX_ENV)
        break;
      namelen = v->nlen;
      skip = 0;
      for (size_t s = 0; s < sh_c; s++) {
        if (namelen == tmplen[s] && smemcmp(v->var, tmpname[s], namelen)) {
          shadowed[skipc++] = v->var;
          skip = 1;
          break;
        }
      }
      if (!skip) {
        lenarr[c] = v->flen;
        len += lenarr[c++];
      }
    }
  }

  size_t arrsize = (c + 1) * sizeof(char *);
  if (!(mem = slalloc(arrsize + (len + 1))))
    return NULL;
  cmd_env = (char **)mem;
  char *buf = (char *)mem + arrsize;
  arr = cmd_env;

  if (sh_env) {
    for (size_t i = 0; i < sh_c; i++) {
      *arr++ = buf;
      memcpy(buf, sh_env[i], lenarr[j]);
      buf += lenarr[j++];
    }
  }
  for (size_t i = 0; i < var_tab_size; i++) {
    size_t skip;
    shvar *v;
    v = &var_tab[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    if (v->flags & VEXPRT) {
      skip = 0;
      for (size_t s = 0; s < skipc; s++) {
        if (v->var == shadowed[s]) {
          skip = 1;
          break;
        }
      }
      if (!skip) {
        size_t var_len;
        *arr++ = buf;
        var_len = lenarr[j++];
        memcpy(buf, v->var, var_len);
        buf += var_len;
      }
    }
  }
  *arr = NULL;
  return cmd_env;
}

/** check if environment has been updated for external commands if so
 * rebuild, otherwise used cached env array */
char **
build_env(char **sh_env)
{
  if (sh_env) {
    return rebuild_env(sh_env);
  }

  if (!env_dirty)
    return env_cache;
  char **env;

  env = rebuild_env(NULL);
  if (env) {
    slfree(env_cache);
    env_cache = env;
    env_dirty = 0;
  }
  return env;
}


/** initialize environment from environ */
void
init_env(void)
{
  size_t i, env_c = 0;
  shvar *p, *ifs;
  int shlvl;
  char *shlvl_s, pwd[PATH_MAX];

  var_count = 0;
  array_len(environ, env_c);
  for (i = 0; i < env_c; i++) {
    char *name, *val;
    read_assn(environ[i], &name, &val);
    setvar(name, val, VEXPRT);
    slfree(name);
    slfree(val);
  }

  if ((p = findvar_n(pathn, 4)))
    p->func = rmchash;
  if (!(ifs = findvar_n(ifsn, 3))) {
    setvar(ifsn, " \t\n", 0);
    ifs = findvar_n(ifsn, 3);
  }
  ifs->func = ifsupdt;
  ifsupdt(shvar_val(ifs));

  sh_pid_s = slalloc(16);
  sh_bgpid_s = slalloc(16);
  sh_ppid_s = slalloc(16);
  if (!sh_pid_s || !sh_bgpid_s || !sh_ppid_s) {
    warn("malloc failed");
    exit(1);
  }
  if (!getcwd(pwd, PATH_MAX))
    shwarnx("getcwd", "couldn't get PWD");
  else
    setvar(pwdn, pwd, VEXPRT);


  sh_ppid = getppid();
  sh_pid = getpid();
  shlvl_s = getvar(shlvln);
  shlvl = (shlvl_s) ? atoi_(shlvl_s) : 0;
  shlvl++;
  lltoa(shlvl, shlvl_s);
  lltoa(sh_pid, sh_pid_s);
  lltoa(sh_ppid, sh_ppid_s);
  setvar(ppidn, sh_ppid_s, VREADONLY);
  setvar(shlvln, shlvl_s, VEXPRT);
  home = getenv(homen);
  homelen = strlen(home);
}

int
exportcmd(char **args)
{
  size_t argc = 0;
  array_len(args, argc);

  if (argc > 1) {
    for (size_t i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;

      eq = strchrnul_(args[i], '=');
      if (*eq == '\0') {
        v = findvar(args[i]);
        if (v) {
          v->flags |= VEXPRT;
          env_dirty = 1;
        } else
          setvar(args[i], NULL, VEXPRT);
      } else {
        read_assn(args[i], &name, &val);
        setvar(name, val, VEXPRT);
        slfree(name);
        slfree(val);
      }
    }
  }

  return 0;
}

int
localcmd(char **argv)
{
  size_t argc = 0;
  array_len(argv, argc);

  if (func_depth == 0) {
    fprintf(stderr, "simpsh: local: can only be used in a function\n"); /*NOLINT*/
    return 1;
  }
  if (argc > 1) {
    for (size_t i = 1; i < argc; i++) {
      char *name, *val;

      st_read_assn(argv[i], &name, &val);
      localvars[localsp++] = grabvar(name);
      if (localsp >= LOCAL_MAX)
        err(1, "local variables exceeded max");
      if (val) {
        setvar(name, val, 0);
      } else {
        val = localvars[localsp - 1].val ? localvars[localsp - 1].val : "";
        setvar(name, val, 0);
      }
    }
  }
  return 0;
}

int
readonlycmd(char **argv)
{
  size_t i, argc = 0;
  array_len(argv,argc);

  if (argc > 1) {
    for (i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;

      eq = strchrnul_(argv[i], '=');
      if (*eq == '\0') {
        v = findvar(argv[i]);
        if (v)
          v->flags |= VREADONLY;
        else
          setvar(argv[i], NULL, VEXPRT);
      } else {
        read_assn(argv[i], &name, &val);
        setvar(name, val, VREADONLY);
        slfree(name);
        slfree(val);
      }
    }
  }

  return 0;
}

int
unsetcmd(char **argv)
{
  shvar *v;
  size_t argc = 0, c;
  unsigned int err;
  char *bargv0;
  array_len(argv, argc);
  enum {
    VARS,
    FUNC,
  } flag;

  flag = 0;
  bargv0 = argv[0];
  ARGBEGIN
  {
    case 'v':
      flag = VARS;
      break;
    case 'f':
      flag = FUNC;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND

  err = 0;
  c = 0;
  if (argc < 1) {
    return 0;

  } else {
    for (; c < argc; c++) {
      if (flag == FUNC) {
        rmfunc(argv[c]);
      } else {
        v = findvar(argv[c]);
        if (v) {
          if (v->flags & VREADONLY) {
            shwarn_arg(bargv0, argv[c], "cannot unset: readonly variable"); /*NOLINT*/
            err = 1;
          } else {
            rmvar(argv[c]);
          }
        }
      }
      if (err)
        return 1;
    }
  }

  return 0;
}
