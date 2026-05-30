#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <err.h>
#include <limits.h>

#include "arg.h"
#include "env.h"
#include "exec.h"
#include "path.h"
#include "main.h"
#include "malloc.h"
#include "opts.h"
#include "var.h"

shvar *var_tab[VAR_BUCKETS];
tmp_var localvars[LOCAL_MAX];
size_t localsp;

/** find variable by name */
__attribute__((hot)) shvar *
findvar(const char *name)
{
  unsigned int i;
  shvar *v;
  size_t nlen;

  nlen = strlen(name);
  i = hash_n(name, nlen, VAR_BUCKETS);
  v = var_tab[i];
  while (v) {
    if (v->var[0] == name[0])
      if (strncmp(v->var, name, nlen) == 0)
        return v;
    v = v->next;
  }
  return NULL;
}

/** set variable value */
void
setvar(char *name, char *val, shvar_flags flags)
{
  if (aflag)
    flags |= VEXPRT;
  shvar *v;
  char *nvar;
  size_t vlen, nlen, i;

  nlen = strlen(name);
  if (!val) {
    nvar = strndup_(name, nlen + 2);
    nvar[nlen] = '=';
    nvar[nlen + 1] = '\0';
  } else {
    vlen = strlen(val);
    if (!(nvar = malloc(nlen + 1 + vlen + 1)))
      return;
    memcpy(nvar, name, nlen);
    nvar[nlen] = '=';
    memcpy(nvar + nlen + 1, val, vlen + 1);
  }

  i = hash_n(name, nlen, VAR_BUCKETS);
  v = var_tab[i];
  while (v) {
    if (v->var[0] == name[0])
      if (strncmp(v->var, name, nlen) == 0 && v->var[nlen] == '=') {
        if (v->flags & VREADONLY) {
          free(nvar);
          return;
        }
        free(v->var);
        v->var = nvar;
        v->flags = flags;
        goto callback;
      }
    v = v->next;
  }
  if (!(v = malloc(sizeof(shvar))))
    return;
  v->var = nvar;
  v->flags = flags;
  v->func = NULL;
  v->next = var_tab[i];
  var_tab[i] = v;

callback:
  if (v->func)
    v->func(name);
  return;
}

/** unset variable */
void
rmvar(const char *name)
{
  unsigned int i;
  shvar **prev;
  shvar *v;
  size_t nlen;

  nlen = strlen(name);
  i = hash_n(name, nlen, VAR_BUCKETS);
  prev = &var_tab[i];
  v = var_tab[i];

  while (v) {
    if (v->var[0] == name[0])
      if (strncmp(v->var, name, nlen) == 0) {
        *prev = v->next;
        free(v->var);
        free(v);
        return;
      }
    prev = &v->next;
    v = v->next;
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

  size_t c, sh_c, j, len, var_len, arrsize;
  size_t namelen, lenarr[MAX_ENV], tmplen[MAX_ENV], skipc;
  char *eq, *buf, *tmpname[MAX_ENV], *shadowed[MAX_ENV];
  char **arr, **cmd_env, **mem;
  shvar *var;
  int skip;

  cmd_env = NULL;
  c = 0;
  j = 0;
  len = 0;
  sh_c = 0;
  skipc = 0;

  if (sh_env) {
    sh_c = array_len(sh_env);
    for (size_t i = 0; i < sh_c; i++) {
      lenarr[c] = strlen(sh_env[i]) + 1;
      eq = strchrnul_(sh_env[i], '=');
      tmpname[i] = sh_env[i];
      tmplen[i] = eq - sh_env[i];
      len += lenarr[c++];
    }
  }
  for (size_t i = 0; i < VAR_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags & VEXPRT) {
        namelen = strchrnul_(var->var, '=') - var->var;
        skip = 0;
        for (size_t s = 0; s < sh_c; s++) {
          if (namelen == tmplen[s] &&
              (memcmp(var->var, tmpname[s], namelen)) == 0) {
            shadowed[skipc++] = var->var;
            skip = 1;
            break;
          }
        }
        if (!skip) {
          lenarr[c] = strlen(var->var) + 1;
          len += lenarr[c++];
        }
      }
      var = var->next;
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
  for (size_t i = 0; i < VAR_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags & VEXPRT) {
        skip = 0;
        for (size_t s = 0; s < skipc; s++) {
          if (var->var == shadowed[s]) {
            skip = 1;
            break;
          }
        }
        if (!skip) {
          *arr++ = buf;
          var_len = lenarr[j++];
          memcpy(buf, var->var, var_len);
          buf += var_len;
        }
      }
      var = var->next;
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
  sh_pid = getpid();
  sh_argc = 0;
  sh_argv = NULL;
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
