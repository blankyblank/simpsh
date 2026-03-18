#include "simpsh.h"

static char *getvar(char *);
static char *var_n(char *, size_t, size_t *, size_t *);
static char *varbrace_n(char *, size_t, size_t *);
static int is_pos(char *);
static char *get_posparam(int);
static char *varstatus(size_t *);

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

int
is_pos(char *var) {
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

char *
get_posparam(int n) {
  if (n == 0)
    return sh_argv0 ? sh_argv0 : "";

  if (n < 0 || n > sh_argc)
    return "";
  return sh_argv[n - 1] ? sh_argv[n - 1] : "";
}

char *
getvar(char *vt) {
  char *var;
  if ((var = getenv(vt)) == NULL)
    var = "";
  return var;
}

static inline char *
var_pid(size_t *var_l) {
  *var_l = strlen(sh_pid_s);
  return sh_pid_s;
}

char *
varstatus(size_t *var_l) {
  static char buf[12];
  snprintf(buf, sizeof(buf), "%d", lstatus);
  *var_l = strlen(buf);
  return buf;
}

char *
var_n(char *args, size_t i, size_t *end, size_t *var_l) {
  char *var, *vt;
  size_t j, vt_l;
  int n;
  for (j = i + 1;; j++) {
    if ((isalnum(args[j]) == 0 && args[j] != '_') || args[j] == '\0') {
      vt_l = j - (i + 1);

      if (vt_l == 0) {
        var = &args[i];
        *var_l = 1;
        break;

      } else {
        vt = strndup(&args[i + 1], vt_l);
        if (is_pos(vt)) {
          n = atoi(vt);
          var = get_posparam(n);
        } else {
          var = getvar(vt);
        }
        free(vt);
        break;
      }
    }
  }

  *end = j - 1;
  *var_l = strlen(var);
  return var;
}

char *
varbrace_n(char *args, size_t i, size_t *end) {
  char *vt, *var;
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
        vt = strndup(&args[i + 1], vt_l);
        if (is_pos(vt)) {
          n = atoi(vt);
          var = get_posparam(n);
        } else {
          var = getvar(vt);
        }
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

      if (args[i + 1] == '$') {
        var = var_pid(&var_l);
        j = i + 1;

      } else if (args[i + 1] == '?') {
        var = varstatus(&var_l);
        j = i + 1;

      } else if (args[i + 1] == '{') {
        if ((var = varbrace_n(args, i, &j)) == NULL)
          goto fail;
        var_l = strlen(var);
        if (p + var_l > bufsize)
          e_args = var_realloc(e_args, p, var_l, &bufsize);

      } else {
        var = var_n(args, i, &j, &var_l);
        if (var_l != 1)
          var_l = strlen(var);
        if (p + var_l > bufsize)
          e_args = var_realloc(e_args, p, var_l, &bufsize);
      }

      memcpy(&e_args[p], var, var_l);
      if (vt) free(vt);
      p += var_l;
      i = j;

    } else {
      e_args[p] = args[i];
      p++;
    }
  }

  e_args[p] = '\0';
  return e_args;

fail:
  fprintf(stderr, "%s: Bad substitution\n", sh_argv0);
  if (e_args) free(e_args);
  return NULL;
}
