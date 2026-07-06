#define _POSIX_C_SOURCE 200809L

#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "arg.h"
#include "env.h"
#include "error.h"
#include "expand.h"
#include "main.h"
#include "opts.h"
#include "path.h"
#include "simd.h"
#include "utils.h"
#include "var.h"

#define INTSIZE 16

/* string literals */
const char oinn[16] = "OPTIND";
const char oargn[16] = "OPTARG";
const char oerrn[16] = "OPTERR";

/* shell variables */
static shvar var_tab_init[VAR_BUCKETS_INIT];
static pid_t shpid;
static pid_t shppid;

GVAR gvar = {
  .var_tab = var_tab_init,
  .var_tab_size = VAR_BUCKETS_INIT,
};

static char **env_cache;
static u8 env_dirty = 1;
static void ifsupdt(const char *);

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

static void
optindupdt(const char *val)
{
  optind = val ? atoi_(val) : 1;
  optoff = -1;
}

void
resize_var_tab(void)
{
  size_t ns;
  shvar *n;

  ns = vartab_size * 2;
  if (!(n = slcalloc(ns, sizeof(shvar))))
    return;

  for (size_t i = 0; i < vartab_size; i++) {
    shvar *v;
    size_t h;
    v = &vartab[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    h = hash_n(v->var, v->nlen, ns);
    while (n[h].var)
      h = (h + 1) % ns;
    n[h] = *v;
  }

  if (vartab != var_tab_init)
    slfree(vartab);
  vartab = n;
  vartab_size = ns;
  memset(varcache, 0, sizeof(varcache));
}

/** find variable by name */
__attribute__((hot)) shvar *
findvar_n(const char *restrict name, size_t nlen)
{
  unsigned int ci, bucket;
  shvar *v, *cv, *end;

  if (nlen == 6 && smemcmp(name, STR("LINENO"), nlen)) {
    size_t ll = lltoa(sh_lineno, linebuf + 7);
    linebuf[6] = '=';
    linebuf[7  + ll] = '\0';
    linevar.var = linebuf;
    linevar.nlen = 6;
    linevar.flags = VREADONLY;
    linevar.func = NULL;
    return &linevar;
  }
  bucket =  hash_n(name, nlen, vartab_size);
  ci = bucket & (VAR_CACHE_S - 1);
  cv = varcache[ci];
  if (cv && cv->var && cv->var != TOMBSTONE && cv->nlen == nlen &&
      smemcmp(cv->var, name, nlen)) {
    return cv;
  }

  end = vartab + vartab_size;
  v = vartab + bucket;
  for (;;) {
    if (!v->var)
      return NULL;
    if (v->var != TOMBSTONE && v->nlen == nlen && smemcmp(v->var, name, nlen)) {
      return v;
    }
    if (++v >= end)
      v = vartab;
  }
}

/** set variable value */
void
setvar(const char *restrict name, const char *restrict val, shvar_flags flags)
{
  if (aflag)
    flags |= VEXPRT;
  shvar *v, *n, *end;
  char *nvar;
  size_t vlen, nlen, flen, ci;

  if (!(nlen = strlen(name)))
    return;
  vlen = val ? strlen(val) : 0;
  flen = nlen + vlen + 2;
  end = vartab + vartab_size;
  v = vartab + hash_n(name, nlen, vartab_size);
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
        if (!(nvar = salloc(flen)))
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
      v = vartab;
  }

  if (!(nvar = salloc(flen)))
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
  v->flags = flags;
  n->func = NULL;
  v = n;
  varcnt++;
  if (varcnt > vartab_size * 7 / 10) {
    resize_var_tab();
    end = vartab + vartab_size;
    v = vartab + hash_n(name, nlen, vartab_size);
    for (;;) {
      if (v->var && v->var != TOMBSTONE && v->nlen == nlen &&
          smemcmp(v->var, name, nlen)) {
        break;
      }
      if (++v >= end)
        v = vartab;
    }
  }

callback:
  ci = hash_n(name, nlen, VAR_CACHE_S);
  varcache[ci] = v;
  if (!(flags & VNOCB) && v->func)
    v->func(val);
}

/** unset variable */
void
rmvar(const char *name)
{
  size_t ci;
  shvar *v, *end;
  size_t nlen;

  nlen = strlen(name);
  end = vartab + vartab_size;
  v = vartab + hash_n(name, nlen, vartab_size);

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
      varcnt--;
      env_dirty = 1;
      ci = hash_n(name, nlen, VAR_CACHE_S);
      if (varcache[ci] == v)
        varcache[ci] = NULL;
      return;
    }
    if (++v >= end)
      v = vartab;
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

  for (size_t i = 0; i < vartab_size; i++) {
    size_t namelen, skip;
    shvar *v;
    v = &vartab[i];
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
  if (!(mem = salloc(arrsize + (len + 1))))
    return NULL;
  cmd_env = mem;
  char *buf = (char *)mem + arrsize;
  arr = cmd_env;

  if (sh_env) {
    for (size_t i = 0; i < sh_c; i++) {
      *arr++ = buf;
      memcpy(buf, sh_env[i], lenarr[j]);
      buf += lenarr[j++];
    }
  }
  for (size_t i = 0; i < vartab_size; i++) {
    size_t skip;
    shvar *v;
    v = &vartab[i];
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

  varcnt = 0;
  array_len(environ, env_c);
  for (i = 0; i < env_c; i++) {
    char *name, *val;
    read_assn(environ[i], &name, &val);
    setvar(name, val, VEXPRT);
    slfree(name);
    slfree(val);
  }

  static const varinit varinit_tab[] = {
    { "IFS= \t\n", 0,      ifsupdt    },
    { defpath,     VEXPRT, rmchash    },
    { "PS1=$ ",    0,      0          },
    { "PS2=> ",    0,      0          },
    { "PS4=+ ",    0,      0          },
    { "OPTIND=1",  VNOCB,  optindupdt },
    { "OPTERR=1",  0,      0          },
  };
  for (size_t i = 0; i < sizeof(varinit_tab) / sizeof(varinit_tab[0]); i++) {
    shvar *v;
    const char *eq;
    size_t nlen;
    eq = strchrnul_(varinit_tab[i].text, '=');
    nlen = eq - varinit_tab[i].text;
    if (!(v = findvar_n(varinit_tab[i].text, nlen))) {
      char name[16];
      memcpy(name, varinit_tab[i].text, nlen);
      name[nlen] = '\0';
      setvar(name, (char *)(eq + 1), varinit_tab[i].flags);
      v = findvar_n(varinit_tab[i].text, nlen);
    }
    if (v && varinit_tab[i].func)
      v->func = varinit_tab[i].func;
  }
  shvar *ifs;
  int shlvl;
  char *shlvl_s, pwd[PATH_MAX];

  if ((ifs = findvar_n(STR("IFS"), 3)))
    ifsupdt(shvar_val(ifs));

  if (!getcwd(pwd, PATH_MAX))
    shwarn("getcwd", "couldn't get PWD");
  else
    setvar(STR("PWD"), pwd, VEXPRT);

  sh_pid_s = salloc(INTSIZE);
  sh_bgpid_s = salloc(INTSIZE);
  sh_ppid_s = salloc(INTSIZE);
  if (!sh_pid_s || !sh_bgpid_s || !sh_ppid_s) {
    sherrx("malloc failed");
    exit(1);
  }
  shppid = getppid();
  shpid = getpid();
  lltoa(shpid, sh_pid_s);
  lltoa(shppid, sh_ppid_s);
  setvar(STR("PPID"), sh_ppid_s, VREADONLY);

  shlvl_s = getvar(STR("SHLVL"));
  shlvl = (shlvl_s) ? atoi_(shlvl_s) : 0;
  shlvl++;
  lltoa(shlvl, shlvl_s);
  setvar(STR("SHLVL"), shlvl_s, VEXPRT);

  if ((home = getenv(STR("HOME"))))
    homelen = strlen(home);
}

int
exportcmd(char **argv)
{
  size_t argc = 0;
  array_len(argv, argc);

  if (argc > 1) {
    for (size_t i = 1; i < argc; i++) {
      char *eq;
      char *name, *val;
      shvar *v;

      eq = strchrnul_(argv[i], '=');
      if (*eq == '\0') {
        if (eq == argv[i]) {
          shwarn_arg(argv[0], argv[1], "not a valid identifier");
          return 1;
        }
        if ((v = findvar(argv[i]))) {
          v->flags |= VEXPRT;
          env_dirty = 1;
        } else {
          setvar(argv[i], NULL, VEXPRT);
        }
      } else {
        if (eq == argv[i]) {
          shwarn_arg(argv[0], argv[1], "not a valid identifier");
          return 1;
        }
        read_assn(argv[i], &name, &val);
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
      localvars[localcnt++] = grabvar(name);
      if (localcnt >= LOCAL_MAX)
        err(1, "local variables exceeded max");
      if (val) {
        setvar(name, val, 0);
      } else {
        val = localvars[localcnt - 1].val ? localvars[localcnt - 1].val : "";
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
  array_len(argv, argc);

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
  if (argc < 1)
    return 0;
  for (; c < argc; c++) {
    if (flag == FUNC) {
      rmfunc(argv[c]);
    } else {
      v = findvar(argv[c]);
      if (v) {
        if (v->flags & VREADONLY) {
          shwarn_arg(bargv0, argv[c],
                     "cannot unset: readonly variable"); /*NOLINT*/
          err = 1;
        } else {
          rmvar(argv[c]);
        }
      }
    }
    if (err)
      return 1;
  }

  return 0;
}
