#include "simpsh.h"
#include "ctype.h"
#include "var.h"
#include "utils.h"

shvar *var_tab[VAR_BUCKETS] = {NULL};

static char *var_n(char *, size_t, size_t *, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
int setvar(const char *, const char *, int);

int
setvar(const char *name, const char *val, int exp) {
  int i;
  shvar *v;
  i = hash(name, VAR_BUCKETS);
  v = var_tab[i];

  while (v) {
    if (strcmp(v->name, name) == 0) {
      free(v->value);
      v->value = strdup(val);
      return 1;
    }
    v = v->next;
  }

  v = malloc(sizeof(shvar));
  v->name = strdup(name);
  v->value = strdup(val);
  v->next = var_tab[i];
  var_tab[i] = v;

  return 1;
}

char *
var_n(char *args, size_t i, size_t *end, size_t *var_l) {
  char *env_var, *vt, *var;
  size_t j, vt_l;
  int n;
  for (j = i + 1;; j++) {
    if ((isalnum(args[j]) == 0 && args[j] != '_') || args[j] == '\0') {
      vt_l = j - (i + 1);

      if (vt_l == 0) {
        env_var = &args[i];
        *var_l = 1;
        break;

      } else {
        vt = strndup(&args[i + 1], vt_l);
        if (is_posparam(vt)) {
          n = atoi(vt);
          env_var = get_posparam(n);
        } else {
          env_var = getvar(vt);
        }
        free(vt);
        break;
      }
    }
  }

  *var_l = strlen(env_var);
  *end = j - 1;
  var = strdup(env_var);
  return var;
}

char *
varbrace_n(const char *args, size_t i, size_t *end) {
  char *vt, *var, *env_var;
  size_t j, vt_l;
  int n;

  if (strchr(args, '}') == NULL)
    return NULL;

  for (j = i + 1;; j++) {
    if (args[j] == '}') {
      vt_l = j - (i + 2);
      if (vt_l == 0)
        return NULL;
      else {
        vt = strndup(&args[i + 2], vt_l);
        if (is_posparam(vt)) {
          n = atoi(vt);
          env_var = get_posparam(n);
        } else {
          env_var = getvar(vt);
        }
        free(vt);
        break;
      }
    }
  }

  *end = j;
  var = strdup(env_var);
  return var;
}

char *
exp_var(char *line, size_t *pos, size_t *len) {
  size_t i = *pos;
  size_t end;
  char *var;

  if (line[i] != '$')
    return NULL;

  if (line[i + 1] == '$') {
    var = var_pid(len);
    *pos = i + 2;
  } else if (line[i + 1] == '?') {
    var = statusvar(len);
    *pos = i + 2;
  } else if (line[i + 1] == '{') {
    var = varbrace_n(line, i, &end);
    if (!var)
      return NULL;
    *len = strlen(var);
    *pos = end + 1;
  } else {
    var = var_n(line, i, &end, len);
    *pos = end + 1;
  }

  return var;
}
