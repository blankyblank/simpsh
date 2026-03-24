#include "simpsh.h"
#include "ctype.h"

static char *var_n(char *, size_t, size_t *, size_t *);
static char *varbrace_n(const char *, size_t, size_t *);
static char *varstatus(size_t *);

static inline char * getvar(const char *vt) {
  char *var;
  if ((var = getenv(vt)) == NULL)
    var = "";
  return var;
}
static inline char * var_pid(size_t *var_l) {
  *var_l = strlen(sh_pid_s);
  return sh_pid_s;
}
static inline char * get_posparam(int n) {
  if (n == 0)
    return sh_argv0 ? sh_argv0 : "";

  if (n < 0 || n > sh_argc)
    return "";
  return sh_argv[n - 1] ? sh_argv[n - 1] : "";
}
static inline int is_posparam(const char *var) {
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
static char * varstatus(size_t *var_l) {
  char *buf = malloc(12);
  snprintf(buf, 12, "%d", lstatus);
  *var_l = strlen(buf);
  return buf;
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
    var = varstatus(len);
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
