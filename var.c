#include "simpsh.h"

static char *getvar(char *);
static char *varn(char *, size_t, size_t *, size_t *);
static char *varbracen(char *, size_t, size_t *);
static inline char *
var_realloc(char *e_args, size_t p, size_t var_l, size_t *bufsize) {
  char *t;

  *bufsize = p + var_l + 256;
  t = realloc(e_args, *bufsize);
  if (!t) {
    free(e_args);
    return NULL;
  }
  e_args = t;
  return e_args;
}

char *
getvar(char *vt) {
  char *var;
  if ((var = getenv(vt)) == NULL)
    var = "";
  return var;
}

char *
varn(char *args, size_t i, size_t *end, size_t *var_l) {
  char *var, *vt;
  size_t j, vt_l;
  for (j = i + 1;; j++) {
    if ((isalnum(args[j]) == 0 && args[j] != '_') || args[j] == '\0') {
      vt_l = j - (i + 1);

      if (vt_l == 0) {
        var = &args[i];
        *var_l = 1;
        break;

      } else {
        vt = strndup(&args[i + 1], vt_l);
        var = getvar(vt);
        free(vt);

        break;
      }
    }
  }

  *end = j - 1;
  return var;
}

char *
varbracen(char *args, size_t i, size_t *end) {
  char *vt, *var;
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
        var = getvar(vt);
        free(vt);
        break;
      }
    }
  }

  *end = j;
  return var;
}

char *
exp_var(char *args) {
  char *var, *e_args, *vt = NULL;
  size_t args_l = strlen(args);
  size_t bufsize = args_l * 2;
  size_t i, j, p = 0;
  size_t var_l;

  e_args = malloc(bufsize);
  if (!e_args) {
    perror("malloc failed");
    return NULL;
  }

  for (i = 0; i < args_l; i++) {
    if (args[i] == '$') {
      if (args[i + 1] == '{') {
        if ((var = varbracen(args, i, &j)) == NULL) {
          fprintf(stderr, "%s: Bad substitution\n", name);
          free(e_args);
          return NULL;
        }
        var_l = strlen(var);
        if (p + var_l > bufsize)
          e_args = var_realloc(e_args, p, var_l, &bufsize);

      } else {
        var = varn(args, i, &j, &var_l);
        if (var_l != 1)
          var_l = strlen(var);
        if (p + var_l > bufsize)
          e_args = var_realloc(e_args, p, var_l, &bufsize);
      }

      memcpy(&e_args[p], var, var_l);
      if (vt)
        free(vt);
      p += var_l;
      i = j;
    } else {
      e_args[p] = args[i];
      p++;
    }
  }

  e_args[p] = '\0';
  return e_args;
}
