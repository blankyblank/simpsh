#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include <err.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "arg.h"
#include "main.h"
#include "env.h"
#include "errmsg.h"
#include "expand.h"
#include "opts.h"
#include "path.h"
#include "pipe.h"
#include "utils.h"
#include "var.h"

#define INTSIZE 24

/* string literals */
const char oinn[] = "OPTIND";
const char oargn[] = "OPTARG";
const char oerrn[] = "OPTERR";
static const char oinvn[] = "OPTIND=1";
static const char oerrvn[] = "OPTERR=1";
static const char ifsvn[] = "IFS= \t\n";
static const char ps1vn[] = "PS1=$ ";
static const char ps2vn[] = "PS2=> ";
static const char ps4vn[] = "PS4=+ ";

/* shell variables */
static shvar var_tab_init[VAR_BUCKETS_INIT];
static pid_t shpid;
static pid_t shppid;
GVAR gvar = {
  .vartab = var_tab_init,
  .vartab_size = VAR_BUCKETS_INIT,
};

static char **env_cache;
static u8 env_dirty = 1;
static void ifsupdt(const char *);
static int cmpvar(const void *, const void *);


void
ifsupdt(const char *ifs)
{
  memset(ifschar, 0, 256);
  if (!ifs)
    ifs = " \t\n";
  gvar.ifsnull = !*ifs;
  gvar.ifsvlen = 3;
  gvar.ifsv[0] = ' ';
  gvar.ifsv[1] = '\t';
  gvar.ifsv[2] = '\n';
  for (; *ifs; ifs++)
    if (!is_ws(*ifs)) {
      ifschar[(unsigned char)*ifs] = 1;
      gvar.ifsv[gvar.ifsvlen++] = *ifs;
    }
}

static void
optindupdt(const char *val)
{
  optind = val ? atoi_(val) : 1;
  OPTOFF = -1;
}

void
resize_var_tab(void)
{
  size_t ns;
  shvar *n;

  ns = VARTAB_SIZE * 2;
  if (!(n = slcalloc(ns, sizeof(shvar))))
    return;

  for (size_t i = 0; i < VARTAB_SIZE; i++) {
    shvar *v;
    size_t h;
    v = &VARTAB[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    h = hash_n(v->var, v->nlen, ns);
    while (n[h].var)
      h = (h + 1) % ns;
    n[h] = *v;
  }

  if (VARTAB != var_tab_init)
    sfree(VARTAB);
  VARTAB = n;
  VARTAB_SIZE = ns;
  memset(VARCACHE, 0, sizeof(VARCACHE));
}

static int
cmpvar(const void *restrict va, const void *restrict vb)
{
  const shvar *j, *k;
  const char *a, *b;
  j = *(const shvar **)va;
  k = *(const shvar **)vb;
  a = j->var;
  b = k->var;
  while (*a && *a != '=' && *b && *b != '=') {
    if (*a != *b)
      return (unsigned char)*a - (unsigned char)*b;
    ++a;
    ++b;
  }
  if (*a == '\0' || *a == '=') {
    if (*b == '\0' || *b == '=')
      return 0;
    return -1;
  }
  return 1;
}

void
printvars(const char *prfx, shvflags mask)
{
  shvar **enva;
  shvar *v;
  size_t c, f;

  c = 0;
  for (size_t i = 0; i < VARTAB_SIZE; i++) {
    v = &VARTAB[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    if (mask && !(v->flags & mask))
      continue;
    c++;
  }
  if (c == 0)
    return;

  enva = st_alloc((c + 1) * sizeof(shvar *));
  f = 0;
  for (size_t i = 0; i < VARTAB_SIZE; i++) {
    v = &VARTAB[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    if (mask && !(v->flags & mask))
      continue;
    enva[f++] = v;
  }
  enva[f] = NULL;
  qsort(enva, f, sizeof(char *), cmpvar);

  for (size_t i = 0; i < f; i++) {
    char *n, *v;
    st_read_assn(enva[i]->var, &n, &v);
    if (mask && enva[i]->flags & VUNSET)
      printf("%s %s\n", prfx, n);
    else
      printf("%s %s=%s\n", prfx, n, quotestrn(v));
  }
  return;
}

/** find variable by name */
__attribute__((hot)) shvar *
findvar_n(const char *restrict name, size_t nlen)
{
  unsigned int ci, bucket;
  shvar *v, *cv, *end;

  if (nlen == 6 && !memcmp(name, "LINENO", nlen)) {
    size_t ll = lltoa(gstate.lineno, gvar.linebuf + 7);
    gvar.linebuf[6] = '=';
    gvar.linebuf[7  + ll] = '\0';
    LINENO.var = gvar.linebuf;
    LINENO.nlen = nlen;
    LINENO.flen = nlen + 1 + ll + 1;
    LINENO.flags = VREADONLY;
    LINENO.func = NULL;
    return &LINENO;
  }
  bucket =  hash_n(name, nlen, VARTAB_SIZE);
  ci = bucket & (VAR_CACHE_S - 1);
  cv = VARCACHE[ci];
  if (cv && cv->var && cv->var != TOMBSTONE && cv->nlen == nlen &&
      !memcmp(cv->var, name, nlen)) {
    return cv;
  }

  end = VARTAB + VARTAB_SIZE;
  v = VARTAB + bucket;
  for (;;) {
    if (!v->var)
      return NULL;
    if (v->var != TOMBSTONE && v->nlen == nlen && !memcmp(v->var, name, nlen)) {
      VARCACHE[ci] = v;
      return v;
    }
    if (++v >= end)
      v = VARTAB;
  }
}

/** set variable value */
void
setvar(const char *restrict name, const char *restrict val, shvflags flags)
{
  if (fakectx) {
    svfkvar(fkstate, name);
  }
  if (aflag)
    flags |= VEXPRT;
  shvar *v, *n, *end;
  char *nvar;
  size_t vlen, nlen, flen, ci;

  if (!(nlen = strlen(name)))
    return;
  vlen = val ? strlen(val) : 0;
  flen = nlen + vlen + 2;
  end = VARTAB + VARTAB_SIZE;
  v = VARTAB + hash_n(name, nlen, VARTAB_SIZE);
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
    } else if (v->nlen == nlen && !memcmp(v->var, name, nlen)) {
      if (v->flags & VREADONLY)
        return;
      if (v->flen >= flen) {
        if (val) {
          memcpy(v->var + nlen + 1, val, vlen + 1);
          v->flen = flen;
        } else {
          v->var[nlen + 1] = '\0';
          v->flen = flen;
        }
      } else {
        if (!(nvar = salloc(flen)))
          return;
        memcpy(nvar, name, nlen);
        nvar[nlen] = '=';
        if (val)
          memcpy(nvar + nlen + 1, val, vlen + 1);
        else
          nvar[nlen + 1] = '\0';
        sfree(v->var);
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
      v = VARTAB;
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
  VARCNT++;
  if (VARCNT > VARTAB_SIZE * 7 / 10) {
    resize_var_tab();
    end = VARTAB + VARTAB_SIZE;
    v = VARTAB + hash_n(name, nlen, VARTAB_SIZE);
    for (;;) {
      if (v->var && v->var != TOMBSTONE && v->nlen == nlen &&
          !memcmp(v->var, name, nlen)) {
        break;
      }
      if (++v >= end)
        v = VARTAB;
    }
  }

callback:
  ci = hash_n(name, nlen, VAR_CACHE_S);
  VARCACHE[ci] = v;
  if (!(flags & VNOCB) && v->func)
    v->func(val);
}

/** unset variable */
void
rmvar(const char *name)
{
  if (fakectx)
    svfkvar(fkstate, name);
  size_t ci;
  shvar *v, *end;
  size_t nlen;

  nlen = strlen(name);
  end = VARTAB + VARTAB_SIZE;
  v = VARTAB + hash_n(name, nlen, VARTAB_SIZE);

  for (;;) {
    if (!v->var)
      return;
    if (v->var != TOMBSTONE && v->nlen == nlen &&
        !memcmp(v->var, name, nlen)) {
      sfree(v->var);
      v->var = TOMBSTONE;
      v->nlen = 0;
      v->flags = 0;
      v->func = NULL;
      VARCNT--;
      env_dirty = 1;
      ci = hash_n(name, nlen, VAR_CACHE_S);
      if (VARCACHE[ci] == v)
        VARCACHE[ci] = NULL;
      return;
    }
    if (++v >= end)
      v = VARTAB;
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

static char **
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
  for (size_t i = 0; i < VARTAB_SIZE; i++) {
    size_t namelen, skip;
    shvar *v;
    v = &VARTAB[i];
    if (!v->var || v->var == TOMBSTONE)
      continue;
    if (v->flags & VEXPRT) {
      if (c >= MAX_ENV)
        break;
      namelen = v->nlen;
      skip = 0;
      for (size_t s = 0; s < sh_c; s++) {
        if (namelen == tmplen[s] && !memcmp(v->var, tmpname[s], namelen)) {
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
  for (size_t i = 0; i < VARTAB_SIZE; i++) {
    size_t skip;
    shvar *v;
    v = &VARTAB[i];
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
    sfree(env_cache);
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
  VARCNT = 0;
  array_len(environ, env_c);

  static const varinit varinit_tab[] = {
    { ifsvn,   0,      ifsupdt    },
    { defpathn, VEXPRT, rmchash    },
    { ps1vn,   0,      0          },
    { ps2vn,   0,      0          },
    { ps4vn,   0,      0          },
    { oinvn,   VNOCB,  optindupdt },
    { oerrvn,  0,      0          },
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

  for (i = 0; i < env_c; i++) {
    char *name, *val;
    read_assn(environ[i], &name, &val);
    setvar(name, val, VEXPRT);
    sfree(name);
    sfree(val);
  }
  shvar *ifs;
  int shlvl;
  char *shlvl_p, shlvl_s[24], pwd[PATH_MAX];

  if ((ifs = findvar_n("IFS", 3)))
    ifsupdt(shvar_val(ifs));

  if (!getcwd(pwd, PATH_MAX))
    shwarn("getcwd", "couldn't get PWD");
  else
    setvar("PWD", pwd, VEXPRT);

  gvar.pid_s = salloc(INTSIZE);
  gvar.bgpid_s = salloc(INTSIZE);
  gvar.ppid_s = salloc(INTSIZE);
  if (!gvar.pid_s || !gvar.bgpid_s || !gvar.ppid_s)
    err(1, "malloc failed");
  shppid = getppid();
  shpid = getpid();
  lltoa(shpid, gvar.pid_s);
  lltoa(shppid, gvar.ppid_s);
  setvar("PPID", gvar.ppid_s, VREADONLY);

  shlvl_p = getvar("SHLVL");
  shlvl = (shlvl_p) ? atoi_(shlvl_p) : 0;
  shlvl++;
  lltoa(shlvl, shlvl_s);
  setvar("SHLVL", shlvl_s, VEXPRT);

  if ((gvar.home = getenv("HOME")))
    gvar.homelen = strlen(gvar.home);
}

int
exportcmd(char **argv)
{
  size_t argc = 0;
  array_len(argv, argc);

  if (argc < 2) {
    printvars("export", VEXPRT);
    return 0;
  }

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
      } else {
        setvar(argv[i], NULL, VEXPRT | VUNSET);
      }
    } else {
      if (eq == argv[i]) {
        shwarn_arg(argv[0], argv[1], "not a valid identifier");
        return 1;
      }
      read_assn(argv[i], &name, &val);
      setvar(name, val,  VEXPRT);
      sfree(name);
      sfree(val);
    }
  }
  env_dirty = 1;
  return 0;
}

int
localcmd(char **argv)
{
  size_t argc = 0;
  array_len(argv, argc);

  if (gstate.funcdepth == 0) {
    fprintf(stderr, "simpsh: local: can only be used in a function\n"); /*NOLINT*/
    return 1;
  }

  if (argc == 1) {
    for (size_t i = 0; i < LOCALCNT; i++) {
      char *val = getvar(LOCALVARS[i].name);
      printf("local %s=%s\n", LOCALVARS[i].name, quotestrn(val ? val : ""));
    }
    return 0;
  }

  for (size_t i = 1; i < argc; i++) {
    char *name, *val;

    st_read_assn(argv[i], &name, &val);
    LOCALVARS[LOCALCNT++] = grabvar(name);
    if (LOCALCNT >= LOCAL_MAX)
      err(1, "local variables exceeded max");
    if (val) {
      setvar(name, val, 0);
    } else {
      val = LOCALVARS[LOCALCNT - 1].val ? LOCALVARS[LOCALCNT - 1].val : "";
      setvar(name, val, 0);
    }
  }
  return 0;
}

int
readonlycmd(char **argv)
{
  size_t i, argc = 0;
  array_len(argv, argc);

  if (argc < 2) {
    printvars("readonly", VREADONLY);
    return 0;
  }

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
        setvar(argv[i], NULL, VREADONLY | VUNSET);
    } else {
      read_assn(argv[i], &name, &val);
      setvar(name, val, VREADONLY);
      sfree(name);
      sfree(val);
    }
  }
  env_dirty = 1;
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
      return bad_opt(argv0, ARGC());
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
