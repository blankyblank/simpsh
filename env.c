/*  env.c - functions surrounding various parts of the shell environment  */
#include "simpsh.h"
#include "env.h"
#include "utils.h"

alias *alias_tab[ALIAS_BUCKETS];
shvar *var_tab[VAR_BUCKETS] = { NULL };
static char *var_n(char *, size_t, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
static char *lookupvar(char *);

static inline char *
getvar(const char *vt)
{
  char *var;
  shvar *v;
  if ((v = find_var(vt))) {
    var = v->value;
  } else if ((var = getenv(vt)) == NULL)
    var = "";
  return var;
}

char **
build_env(char **sh_env)
{
  /* way too complicated way of trying to copy strings into
   * an array of strings. for performance reasons... I guess.*/

  size_t c = 0, sh_c = 0, len = 0;
  size_t name_len, val_len;
  unsigned int i;
  char **arr, **cmd_env = NULL;
  char *buf;
  shvar *var;

  for (i = 0; i < VAR_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags.exported) {
        len += strlen(var->name) + strlen(var->value) + 2;
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

  for (i = 0; i < VAR_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags.exported) {
        *arr++ = buf;
        name_len = strlen(var->name);
        val_len = strlen(var->value);
        memcpy(buf, var->name, name_len);
        buf += name_len;
        *buf++ = '=';
        memcpy(buf, var->value, val_len + 1);
        buf += val_len + 1;
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

void
init_env(void)
{
  size_t i, env_c;
  char *name, *val;

  env_c = array_len(environ);

  for (i = 0; i < env_c; i++) {
    read_assn(environ[i], &name, &val);
    shvar_flag flags = { .exported = 1,
                         .readonly = 0,
                         .null = (val[0] == '\0') };
    setvar(name, val, flags);
    free(name);
    free(val);
  }
}

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
        vt = strndup(&args[i + 1], vt_l);
        env_var = lookupvar(vt);
        break;
      }
    }
  }

  *end = j;
  var = strdup(env_var);
  return var;
}

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
        vt = strndup(&args[i + 2], vt_l);
        env_var = lookupvar(vt);
        break;
      }
    }
  }

  *end = j;
  var = strdup(env_var);
  return var;
}

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
  free(vt);

  return var;
}

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

shvar *
find_var(const char *name)
{
  unsigned int i = hash(name, VAR_BUCKETS);
  shvar *v = var_tab[i];

  while (v) {
    if (strcmp(v->name, name) == 0)
      return v;
    v = v->next;
  }
  return NULL;
}

void
setvar(const char *name, const char *val, shvar_flag flags)
{
  shvar *v;
  char *tmp;

  v = find_var(name);
  if (v) {
    tmp = v->value;
    v->value = strdup(val);
    v->flags = flags;
    free(tmp);
  } else {
    v = malloc(sizeof(shvar));
    v->name = strdup(name);
    v->value = strdup(val);
    v->flags = flags;
    unsigned int i = hash(name, VAR_BUCKETS);
    v->next = var_tab[i];
    var_tab[i] = v;
  }
}

void
unset_var(const char *name)
{
  unsigned int i = hash(name, VAR_BUCKETS);
  shvar **prev = &var_tab[i];
  shvar *v = var_tab[i];

  while (v) {
    if (strcmp(v->name, name) == 0) {
      if (v->flags.exported)
        unsetenv(v->name);
      *prev = v->next;
      free(v->name);
      free(v->value);
      free(v);
      return;
    }
    prev = &v->next;
    v = v->next;
  }
}

alias *
find_alias(const char *name)
{
  unsigned int i = hash(name, ALIAS_BUCKETS);
  alias *a = alias_tab[i];

  while (a) {
    if (strcmp(a->name, name) == 0)
      return a;
    a = a->next;
  }
  return NULL;
}

void
set_alias(const char *name, const char *val)
{
  alias *a;

  a = find_alias(name);
  if (a) {
    free(a->value);
    a->value = strdup(val);
  } else {
    a = malloc(sizeof(alias));
    a->name = strdup(name);
    a->value = strdup(val);
    unsigned int i = hash(name, ALIAS_BUCKETS);
    a->next = alias_tab[i];
    alias_tab[i] = a;
  }
}

void
rm_alias(const char *name)
{
  unsigned int i = hash(name, ALIAS_BUCKETS);
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
