#ifndef VAR_H
#define VAR_H

#include "ctype.h"

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};
typedef struct shvar shvar;
struct shvar {
  char *name;
  char *value;
  shvar *next;
};

#define VAR_BUCKETS 39
#define ALIAS_BUCKETS 64
#define MAX_ALIAS_DEPTH 10
extern shvar *var_tab[VAR_BUCKETS];
extern alias *alias_tab[ALIAS_BUCKETS];

extern char *exp_var(char *, size_t, size_t *);
extern alias *find_alias(const char *);
extern void set_alias(const char *, const char *);
extern void rm_alias(const char *);
void setvar(const char *name, const char *val);
void unset_var(const char *name);
shvar * find_var(const char *name);

static inline char *
statusvar(void) {
  char *buf = malloc(12);
  snprintf(buf, 12, "%d", lstatus);
  return buf;
}


static inline char *
var_pid(void) {
  return strdup(sh_pid_s);
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

// vim: set filetype=c:
#endif /* VAR_H */

