/*  env.c - functions surrounding various parts of the shell environment  */
#include "main.h"
#include "env.h"
#include "utils.h"
#include <ctype.h>

alias *alias_tab[ENV_BUCKETS];
shvar *var_tab[ENV_BUCKETS] = { NULL };
static char *var_n(char *, size_t, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
static char *lookupvar(char *);

/** get the variable for the return status of last command */
static inline char *
statusvar(void)
{
  char *buf = st_alloc(12);
  snprintf(buf, 12, "%d", lstatus);
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
is_posparam(const char *var)
{
  size_t i;
  size_t var_l = strlen(var);

  if (!var || !*var)
    return 0;
  for (i = 0; i < var_l; i++) {
    if (!isdigit(var[i]))
      return 0;
  }
  return 1;
}

/** get variable value from name */
static inline char *
getvar(const char *vt)
{
  char *var;
  shvar *v;
  if ((v = find_var(vt))) {
    var = shvar_val(v);
  } else if ((var = getenv(vt)) == NULL)
    var = "";
  return var;
}

/** build environment array for exec */
char **
build_env(char **sh_env)
{
  /* way too complicated way of trying to copy strings into
   * an array of strings. for performance reasons... I guess.*/

  size_t c = 0, sh_c = 0, len = 0;
  size_t var_len;
  unsigned int i;
  char **arr, **cmd_env = NULL;
  char *buf;
  shvar *var;

  for (i = 0; i < ENV_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags.exported) {
        len += strlen(var->var) + 1;
        c++;
      }
      var = var->next;
    }
  }
  if (sh_env) {
    sh_c = array_len(sh_env);
    for (i = 0; i < sh_c; i++) {
      if (sh_env[i]) {
        len += strlen(sh_env[i]) + 1;
      }
    }
  }

  c += sh_c;
  cmd_env = malloc((c + 1) * sizeof(char *) + sizeof(char *));
  buf = malloc(len + 1);
  *(char **)cmd_env = buf;
  arr = cmd_env + 1;

  for (i = 0; i < ENV_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags.exported) {
        *arr++ = buf;
        var_len = strlen(var->var);
        memcpy(buf, var->var, var_len + 1);
        buf += var_len + 1;
      }
      var = var->next;
    }
  }

  if (sh_env) {
    for (i = 0; i < sh_c; i++) {
      *arr++ = buf;
      memcpy(buf, sh_env[i], strlen(sh_env[i]) + 1);
      buf += strlen(sh_env[i]) + 1;
    }
  }
  *arr = NULL;
  return cmd_env + 1;
}

/** initialize environment from environ */
void
init_env(void)
{
  size_t i, env_c;
  shvar_flags flags = EXPRT;

  env_c = array_len(environ);

  for (i = 0; i < env_c; i++) {
    char *name, *val;
    read_assn(environ[i], &name, &val);
    setvar(name, val, flags);
  }
}

/** expand $var style variables */
char *
var_n(char *args, size_t i, size_t *end)
{
  char *env_var, *vt, *var;
  size_t j, vt_l;

  for (j = i + 1;; j++) {
    if ((isalnum(args[j]) == 0 && args[j] != '_') || args[j] == '\0') {
      vt_l = j - (i + 1);

      if (vt_l == 0) {
        env_var = &args[i];
        break;
      } else {
        vt = st_strndup(&args[i + 1], vt_l);
        env_var = lookupvar(vt);
        break;
      }
    }
  }

  *end = j;
  var = st_strdup(env_var);
  return var;
}

/** expand ${var} style variables */
char *
varbrace_n(const char *args, size_t i, size_t *end)
{
  char *vt, *var, *env_var;
  size_t j, vt_l;

  if (strchr(args, '}') == NULL)
    return NULL;

  for (j = i + 1;; j++) {
    if (args[j] == '}') {
      vt_l = j - (i + 2);
      if (vt_l == 0)
        return NULL;
      else {
        vt = st_strndup(&args[i + 2], vt_l);
        env_var = lookupvar(vt);
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
lookupvar(char *vt)
{
  char *var;
  int n;

  if (is_posparam(vt)) {
    n = atoi(vt);
    var = get_posparam(n);
  } else {
    var = getvar(vt);
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
  unsigned int i = hash(name, ENV_BUCKETS);
  shvar *v = var_tab[i];

  while (v) {
    if (strncmp(v->var, name, strlen(name)) == 0)
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
    nvar = malloc(nlen + 1 + vlen + 1);
    memcpy(nvar, name, nlen);
    nvar[nlen] = '=';
    memcpy(nvar + nlen + 1, val, vlen + 1);
  }

  i = hash(name, ENV_BUCKETS);
  v = var_tab[i];
  while (v) {
    if (memcmp(v->var, name, nlen) == 0 && v->var[nlen] == '=') {
      if (v->flags.readonly) {
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
  v = malloc(sizeof(shvar));
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
  unsigned int i = hash(name, ENV_BUCKETS);
  shvar **prev = &var_tab[i];
  shvar *v = var_tab[i];
  size_t len;

  len = strlen(name);
  while (v) {
    if (strncmp(v->var, name, len) == 0) {
      if (v->flags.exported)
        unsetenv(name);
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
  unsigned int i = hash(name, ENV_BUCKETS);
  alias *a = alias_tab[i];

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

  a = find_alias(name);
  if (a) {
    free(a->value);
    a->value = s_strdup(val);
  } else {
    a = malloc(sizeof(alias));
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
  unsigned int i = hash(name, ENV_BUCKETS);
  alias **prev = &alias_tab[i];
  alias *a = alias_tab[i];

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
