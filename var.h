#ifndef VAR_H
#define VAR_H

#define VAR_BUCKETS 39

typedef struct shvar shvar;
struct shvar {
  char *name;
  char *value;
  shvar *next;
};

extern shvar *var_tab[VAR_BUCKETS];

static inline char *
getvar(const char *vt) {
  char *var;
  if ((var = getenv(vt)) == NULL)
    var = "";
  return var;
}

static char *
statusvar(size_t *var_l) {
  char *buf = malloc(12);
  snprintf(buf, 12, "%d", lstatus);
  *var_l = strlen(buf);
  return buf;
}


static inline char *
var_pid(size_t *var_l) {
  *var_l = strlen(sh_pid_s);
  return sh_pid_s;
}

static inline char *
get_posparam(int n) {
  if (n == 0)
    return sh_argv0 ? sh_argv0 : "";

  if (n < 0 || n > sh_argc)
    return "";
  return sh_argv[n - 1] ? sh_argv[n - 1] : "";
}

static inline int
is_posparam(const char *var) {
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

#endif /* VAR_H */

