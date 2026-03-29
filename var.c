#include "simpsh.h"
#include "var.h"
#include "utils.h"

shvar *var_tab[VAR_BUCKETS] = {NULL};

static char *var_n(char *, size_t, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
void setvar(const char *, const char *);
// int setvar(const char *, const char *, int);
static char * lookupvar (char *);

char *
var_n(char *args, size_t i, size_t *end) {
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
varbrace_n(const char *args, size_t i, size_t *end) {
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

char * lookupvar (char * vt) {
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
exp_var(char *word, size_t s, size_t *e) {
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
find_var(const char *name) {
  shvar *v;
  HASH_LOOKUP(var_tab, VAR_BUCKETS, name, name, shvar, v);
  return v;
}

void
setvar(const char *name, const char *val) {
  HASH_INSERT(var_tab, VAR_BUCKETS, name, val, shvar, name, value);
}

void
unset_var(const char *name) {
  HASH_DELETE(var_tab, VAR_BUCKETS, name, shvar, name, value);
}

// int
// setvar(const char *name, const char *val, int exp) {
//   int i;
//   shvar *v;
//   i = hash(name, VAR_BUCKETS);
//   v = var_tab[i];
//
//   while (v) {
//     if (strcmp(v->name, name) == 0) {
//       free(v->value);
//       v->value = strdup(val);
//       return 1;
//     }
//     v = v->next;
//   }
//
//   v = malloc(sizeof(shvar));
//   v->name = strdup(name);
//   v->value = strdup(val);
//   v->next = var_tab[i];
//   var_tab[i] = v;
//
//   return 1;
// }

