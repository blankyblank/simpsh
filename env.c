/*  env.c - functions surrounding various parts of the shell environment  */
#include "malloc.h"
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <linux/limits.h>
#include <stddef.h>
#include <string.h>

#include "env.h"
#include "lex.h"
#include "main.h"
#include "utils.h"

alias *alias_tab[ENV_BUCKETS];
shvar *var_tab[ENV_BUCKETS];
shfunc *func_tab[ENV_BUCKETS];
static char *var_n(const char *, size_t, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
static char *lookupvar(const char *, size_t);
static cmd_tree *tree_dup(cmd_tree *);

/** find variable by name */
shvar *
findvar(const char *name)
{
  unsigned int i;
  shvar *v;
  size_t namelen;

  i = hash(name, ENV_BUCKETS);
  v = var_tab[i];
  namelen = strlen(name);
  while (v) {
    if (v->var[0] == name[0])
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
    if (v->var[0] == name[0])
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
rmvar(const char *name)
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
    if (v->var[0] == name[0])
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

/** get the pid shell variable */
#define varpid() (st_strdup(sh_pid_s))
#define varargc() \
  char *buf; \
  buf = st_alloc(16); \
  snprintf(buf, 16, "%d", sh_argc); \
  return buf;

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
        env_var = NULL;
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

static inline redir *
redirdup(redir *s)
{
  redir *n;
  if (!s)
    return NULL;
  n = malloc(sizeof(redir));
  n->fd = s->fd;
  n->name = wfdup(s->name);
  n->type = s->type;
  n->next = redirdup(s->next);
  return n;
}


static cmd_tree *
tree_dup(cmd_tree *s)
{
  cmd_tree *n;
  size_t cnt;

  if (!s)
    return NULL;

  n = malloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = s->type;
  n->flags = s->flags;

  switch (n->type) {
    case OP:
      COPP(n) = COPP(s);
      n->left = tree_dup(s->left);
      n->right = tree_dup(s->right);
      break;
    case SUBSHELL:
      n->left = tree_dup(s->left);
      break;
    case FUNC:
      CFUNC(n) = wfdup(CFUNC(s));
      n->left = tree_dup(s->left);
      break;
    case REDIR:
      CREDR(n) = redirdup(CREDR(s));
      n->left = tree_dup(s->left);
      break;

    case CMD:
      CREDR(n) = redirdup(CREDR(s));
      cnt = 0;
      for (size_t i = 0; CARGS(s)[i]; i++)
        cnt++;
      CARGS(n) = malloc((cnt + 1) * sizeof(wf *));
      for (size_t i = 0; i < cnt; i++)
        CARGS(n)[i] = wfdup(CARGS(s)[i]);
      CARGS(n)[cnt] = NULL;
      if (CVARS(s)) {
        cnt = 0;
        for (size_t i = 0; CVARS(s)[i]; i++)
          cnt++;
        CVARS(n) = malloc((cnt + 1) * sizeof(char *));
        for (size_t i = 0; i < cnt; i++)
          CVARS(n)[i] = s_strdup(CVARS(s)[i]);
        CVARS(n)[cnt] = NULL;
      } else {
        CVARS(n) = NULL;
      }
  }
  return n;
}

shfunc *
findfunc(const char *name)
{
  size_t i;
  shfunc *f;
  i = hash(name, ENV_BUCKETS);
  f = func_tab[i];

  while (f) {
    if (f->name[0] == name[0])
      if ((strcmp(f->name, name)) == 0)
        return f;
    f = f->next;
  }
  return NULL;
}

void
setfunc(const char *name, cmd_tree *body)
{
  shfunc *f, *n;
  size_t i;

  f = findfunc(name);
  if (f) {
    free_tree(f->body);
    free(f->name);
    f->name = s_strdup(name);
    f->body = tree_dup(body);
  } else {
    i = hash(name, ENV_BUCKETS);
    n = malloc(sizeof(shfunc));
    if (!n)
      return;
    n->name = s_strdup(name);
    n->body = tree_dup(body);
    n->next = func_tab[i];
    func_tab[i] = n;
  }

}

/** find alias by name */
alias *
findalias(const char *name)
{
  unsigned int i;
  alias *a;
  i = hash(name, ENV_BUCKETS);
  a = alias_tab[i];

  while (a) {
    if (a->name[0] == name[0])
      if (strcmp(a->name, name) == 0)
        return a;
    a = a->next;
  }
  return NULL;
}

/** set alias value */
void
setalias(const char *name, const char *val)
{
  alias *a;

  if ((a = findalias(name))) {
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

/** remove function */
void
rmfunc(const char *name)
{
  size_t i;
  shfunc **prev;
  shfunc *f;

  i = hash(name, ENV_BUCKETS);
  prev = &func_tab[i];
  f = func_tab[i];
  while (f) {
    if (f->name[0] == name[0])
      if (strcmp(f->name, name) == 0) {
        *prev = f->next;
        free(f->name);
        free_tree(f->body);
        free(f);
        return;
      }
    prev = &f->next;
    f = f->next;
  }
}

/** remove alias */
void
rmalias(const char *name)
{
  size_t i;
  alias **prev;
  alias *a;

  i = hash(name, ENV_BUCKETS);
  prev = &alias_tab[i];
  a = alias_tab[i];
  while (a) {
    if (a->name[0] == name[0])
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
      eq = s_strchrnul(sh_env[i], '=');
      tmpname[i] = sh_env[i];
      tmplen[i] = eq - sh_env[i];
      len += lenarr[c++];
    }
  }
  for (size_t i = 0; i < ENV_BUCKETS; i++) {
    var = var_tab[i];
    while (var) {
      if (var->flags & VEXPRT) {
        namelen = s_strchrnul(var->var, '=') - var->var;
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
  for (size_t i = 0; i < ENV_BUCKETS; i++) {
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

  env_c = array_len(environ);
  for (i = 0; i < env_c; i++) {
    char *name, *val;
    read_assn(environ[i], &name, &val);
    setvar(name, val, VEXPRT);
    free(name);
    free(val);
  }

  sh_argv0 = "simpsh";
  sh_pid_s = malloc(16);
  sh_pid = getpid();
  sh_argc = 0;
  sh_argv = NULL;
  snprintf(sh_pid_s, 16, "%d", sh_pid);
  home = getenv("HOME");
}

