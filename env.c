/*  env.c - functions surrounding various parts of the shell environment  */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>

#include "main.h"
#include "env.h"
#include "utils.h"

alias *alias_tab[ENV_BUCKETS];
shvar *var_tab[ENV_BUCKETS] = { NULL };
static char *var_n(const char *, size_t, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
static char *lookupvar(const char *, size_t);

/** get the variable for the return status of last command */
static inline char *
statusvar(void)
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

/** get the pid shell variable */
static inline char *
var_pid(void)
{
  return st_strdup(sh_pid_s);
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
    if (!isdigit(var[i]))
      return 0;
  }
  return 1;
}



/** build environment array for exec */
char **
build_env(char **sh_env)
{
  /* way too complicated way of trying to copy strings into
   * an array of strings. for performance reasons... I guess.*/

  size_t c, sh_c, j, len, var_len, arrsize;
  size_t lenarr[MAX_ENV];
  char *buf;
  char **arr, **cmd_env = NULL, **mem;
  shvar *var;

  c = 0;
  j = 0;
  len = 0;
  sh_c = 0;
  for (size_t i = 0; i < ENV_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags & VEXPRT) {
        lenarr[c] = strlen(var->var) + 1;
        len += lenarr[c++];
      }
      var = var->next;
    }
  }
  if (sh_env) {
    sh_c = array_len(sh_env);
    for (size_t i = 0; i < sh_c; i++) {
      lenarr[c] = strlen(sh_env[i]) + 1;
      len += lenarr[c++];
    }
  }

  arrsize = (c + 1) * sizeof(char *);
  if (!(mem = malloc(arrsize + (len + 1))))
    return NULL;
  cmd_env = (char **)mem;
  buf = (char *)mem + arrsize;
  arr = cmd_env;

  for (size_t i = 0; i < ENV_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags & VEXPRT) {
        *arr++ = buf;
        var_len = lenarr[j++];
        memcpy(buf, var->var, var_len);
        buf += var_len;
      }
      var = var->next;
    }
  }
  if (sh_env) {
    for (size_t i = 0; i < sh_c; i++) {
      *arr++ = buf;
      memcpy(buf, sh_env[i], lenarr[j]);
      buf += lenarr[j++];
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
  env_c = array_len(environ);
  for (i = 0; i < env_c; i++) {
    char *name, *val;
    read_assn(environ[i], &name, &val);
    setvar(name, val, VEXPRT);
    free(name);
    free(val);
  }
}

/** expand $var style variables */
char *
var_n(const char *args, size_t i, size_t *end)
{
  const char *env_var;
  char *var;
  size_t j, vt_l;

  for (j = i + 1;; j++) {
    if ((isalnum(args[j]) == 0 && args[j] != '_') || args[j] == '\0') {
      vt_l = j - (i + 1);

      if (vt_l == 0) {
        env_var = &args[i];
        break;
      } else {
        env_var = lookupvar(&args[i + 1], vt_l);
        break;
      }
    }
  }

  *end = j;
  if (!env_var)
    return NULL;
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
    i = hash(vt, ENV_BUCKETS);
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

char * exp_tilde(char *word, size_t s, size_t *e) {
  char *hm;

  if (word[s] != '~')
    return NULL;
  if (s == 0 && (word[s + 1] == '\0' || word[s + 1] == '/')) {
    if (!(hm = getvar("HOME")))
      return NULL;
    *e = s + 1;
    return st_strdup(hm);
  }
  return NULL;
}

/** expand variables in word */
char *
exp_var(char *word, size_t s, size_t *e)
{
  if (word[s] != '$')
    return NULL;

  if (word[s + 1] == '$') {
    *e = s + 2;
    return var_pid();
  } else if (word[s + 1] == '?') {
    *e = s + 2;
    return statusvar();
  } else if (word[s + 1] == '{') {
    return varbrace_n(word, s, e);
  } else {
    return var_n(word, s, e);
  }
}

/** find variable by name */
shvar *
find_var(const char *name)
{
  unsigned int i;
  shvar *v;
  size_t namelen;

  i = hash(name, ENV_BUCKETS);
  v = var_tab[i];
  namelen = strlen(name);
  while (v) {
    if (strncmp(v->var, name, namelen) == 0)
      return v;
    v = v->next;
  }
  return NULL;
}

/** set variable value */
void
setvar(char *name, char *val, shvar_flags flags)
{
  shvar *v;
  char *nvar;
  size_t vlen, nlen, i;

  nlen = strlen(name);
  if (!val) {
    nvar = s_strndup(name, nlen + 2);
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

  i = hash(name, ENV_BUCKETS);
  v = var_tab[i];
  while (v) {
    if (strncmp(v->var, name, nlen) == 0 && v->var[nlen] == '=') {
      if (v->flags & VREADONLY) {
        free(nvar);
        return;
      }
      free(v->var);
      v->var = nvar;
      v->flags = flags;
      return;
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
}

/** unset variable */
void
unset_var(const char *name)
{
  unsigned int i;
  shvar **prev;
  shvar *v;
  size_t len;

  i = hash(name, ENV_BUCKETS);
  prev = &var_tab[i];
  v = var_tab[i];

  len = strlen(name);
  while (v) {
    if (strncmp(v->var, name, len) == 0) {
      *prev = v->next;
      free(v->var);
      free(v);
      return;
    }
    prev = &v->next;
    v = v->next;
  }
}

/** find alias by name */
alias *
find_alias(const char *name)
{
  unsigned int i;
  alias *a;
  i = hash(name, ENV_BUCKETS);
  a = alias_tab[i];

  while (a) {
    if (strcmp(a->name, name) == 0)
      return a;
    a = a->next;
  }
  return NULL;
}

/** set alias value */
void
set_alias(const char *name, const char *val)
{
  alias *a;

  if ((a = find_alias(name))) {
    free(a->value);
    a->value = s_strdup(val);
  } else {
    if (!(a = malloc(sizeof(alias))))
      return;
    a->name = s_strdup(name);
    a->value = s_strdup(val);
    unsigned int i = hash(name, ENV_BUCKETS);
    a->next = alias_tab[i];
    alias_tab[i] = a;
  }
}

/** remove alias */
void
rm_alias(const char *name)
{
  unsigned int i;
  alias **prev;
  alias *a;

  i = hash(name, ENV_BUCKETS);
  prev = &alias_tab[i];
  a = alias_tab[i];
  while (a) {
    if (strcmp(a->name, name) == 0) {
      *prev = a->next;
      free(a->name);
      free(a->value);
      free(a);
      return;
    }
    prev = &a->next;
    a = a->next;
  }
}
