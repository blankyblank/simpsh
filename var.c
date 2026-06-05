#define _POSIX_C_SOURCE 200809L

#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "arg.h"
#include "env.h"
#include "exec.h"
#include "main.h"
#include "malloc.h"
#include "opts.h"
#include "path.h"
#include "utils.h"
#include "var.h"

static shvar var_tab_init[VAR_BUCKETS_INIT];
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
  if (!(n = calloc(ns, sizeof(shvar))))
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
    free(var_tab);
  var_tab = n;
  var_tab_size = ns;
  memset(var_cache, 0, sizeof(var_cache));
}

/** find variable by name */
__attribute__((hot)) shvar *
findvar(const char *name)
{
  unsigned int i, ci;
  shvar *v, *cv;
  size_t nlen;

  nlen = strlen(name);

  ci = hash_n(name, nlen, VAR_CACHE_S);
  cv = var_cache[ci];
  if (cv && cv->var && cv->var != TOMBSTONE && cv->nlen == nlen &&
      memcmp(cv->var, name, nlen) == 0)
    return cv;

  i = hash_n(name, nlen, var_tab_size);
  for (;;) {
    v = &var_tab[i];
    if (!v->var)
      return NULL;
    if (v->var != TOMBSTONE && v->nlen == nlen &&
        memcmp(v->var, name, nlen) == 0) {
      var_cache[ci] = v;
      return v;
    }
    i = (i + 1) % var_tab_size;
  }
}

/** set variable value */
void
setvar(char *restrict name, char *restrict val, shvar_flags flags)
{
  if (aflag)
    flags |= VEXPRT;
  shvar *v, *n;
  char *nvar;
  size_t vlen, nlen, i, ci;

  nlen = strlen(name);
  vlen = val ? strlen(val) : 0;
  if (!(nvar = malloc(nlen + vlen + 2)))
    return;
  memcpy(nvar, name, nlen);
  nvar[nlen] = '=';
  if (val)
    memcpy(nvar + nlen + 1, val, vlen + 1);
  else
   nvar[nlen + 1] = '\0';

  i = hash_n(name, nlen, var_tab_size);
  n = NULL;

  for (;;) {
    v = &var_tab[i];
    if (!v->var) {
      if (!n)
        n = v;
      break;
    }
    if (v->var == TOMBSTONE) {
      if (!n)
        n = v;
    } else if (v->nlen == nlen && memcmp(v->var, name, nlen) == 0) {
      if (v->flags & VREADONLY) {
        free(nvar);
        return;
      }
      free(v->var);
      v->var = nvar;
      v->nlen = nlen;
      v->flags = flags;
      goto callback;
    }
    i = (i + 1) % var_tab_size;
  }

  n->var = nvar;
  n->nlen = nlen;
  n->flags = flags;
  n->func = NULL; // XXX: ???
  v = n;
  var_count++;
  if (var_count > var_tab_size * 7 / 10)
    resize_var_tab();

callback:
  ci = hash_n(name, nlen, VAR_CACHE_S);
  var_cache[ci] = v;
  if (v->func)
    v->func(name);
  return;
}

/** unset variable */
void
rmvar(const char *name)
{
  size_t i, ci;
  shvar *v;
  size_t nlen;

  nlen = strlen(name);
  i = hash_n(name, nlen, var_tab_size);

  for (;;) {
    v = &var_tab[i];
    if (!v->var)
      return;
    if (v->var != TOMBSTONE && v->nlen == nlen &&
        memcmp(v->var, name, nlen) == 0) {
      free(v->var);
      v->var = TOMBSTONE;
      v->nlen = 0;
      v->flags = 0;
      v->func = NULL;
      var_count--;
      ci = hash_n(name, nlen, VAR_CACHE_S);
      if (var_cache[ci] == v)
        var_cache[ci] = NULL;
      return;
    }
    i = (i + 1) % var_tab_size;
  }
}

tmp_var
grabvar(char *name)
{
  tmp_var tmp;
  shvar *v;

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

/** build environment array for exec */
char **
build_env(char **sh_env)
{
  /* way too complicated way of trying to copy strings into
   * an array of strings. for performance reasons... I guess.*/

  size_t c, sh_c, j, len, arrsize;
  size_t lenarr[MAX_ENV], tmplen[MAX_ENV], skipc;
  char *buf, *tmpname[MAX_ENV], *shadowed[MAX_ENV];
  char **arr, **cmd_env, **mem;

  cmd_env = NULL;
  c = 0;
  j = 0;
  len = 0;
  sh_c = 0;
  skipc = 0;

  if (sh_env) {
    sh_c = array_len(sh_env);
    for (size_t i = 0; i < sh_c; i++) {
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
      namelen = v->nlen;
      skip = 0;
      for (size_t s = 0; s < sh_c; s++) {
        if (namelen == tmplen[s] &&
            (memcmp(v->var, tmpname[s], namelen)) == 0) {
          shadowed[skipc++] = v->var;
          skip = 1;
          break;
        }
      }
      if (!skip) {
        lenarr[c] = strlen(v->var) + 1;
        len += lenarr[c++];
      }
    }
  }

  arrsize = (c + 1) * sizeof(char *);
  if (!(mem = malloc(arrsize + (len + 1))))
    return NULL;
  cmd_env = (char **)mem;
  buf = (char *)mem + arrsize;
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

/** initialize environment from environ */
void
init_env(void)
{
  size_t i, env_c;
  shvar *p;

  var_count = 0;
  env_c = array_len(environ);
  for (i = 0; i < env_c; i++) {
    char *name, *val;
    read_assn(environ[i], &name, &val);
    setvar(name, val, VEXPRT);
    free(name);
    free(val);
  }

  if ((p = findvar("PATH")))
    p->func = rmchash;
  sh_argv0 = "simpsh";
  sh_pid_s = malloc(16);
  sh_bgpid_s = malloc(16);
  sh_pid = getpid();
  sh_argc = 0;
  sh_argv = NULL;
  sh_bgpid = 0;
  snprintf(sh_pid_s, 16, "%d", sh_pid);
  home = getenv("HOME");
}

int
exportcmd(char **args)
{
  size_t argc;
  argc = array_len(args);

  /* nvar = s_strndup(args[i], nvarlen + 1); */ 
  if (argc > 1) {
    for (size_t i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;

      eq = strchrnul_(args[i], '=');
      if (*eq == '\0') {
        v = findvar(args[i]);
        if (v)
          v->flags |= VEXPRT;
        else
          setvar(args[i], NULL, VEXPRT);
      } else {
        read_assn(args[i], &name, &val);
        setvar(name, val, VEXPRT);
        free(name);
        free(val);
      }
    }
  }

  return 0;
}

int
localcmd(char **argv)
{
  size_t argc;

  argc = array_len(argv);

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
  int argc, i;
  argc = array_len(argv);

  /* nvar = s_strndup(args[i], nvarlen + 1); */ 
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
        free(name);
        free(val);
      }
    }
  }

  return 0;
}

int
unsetcmd(char **argv)
{
  shvar *v;
  size_t argc, c;
  unsigned int err;
  char *bargv0;
  argc = array_len(argv);
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
      bad_opt(sh_argv0, argv0, ARGC());
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
