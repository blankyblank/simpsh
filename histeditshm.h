#ifndef HISTEDITSHM_H
#define HISTEDITSHM_H

#ifdef STATICLIBEDIT
#include <histedit.h>
#else
#include <dlfcn.h>
#endif /* STATICLIBEDIT */
#include <wchar.h>

#if defined(__GNUC__) || defined(__clang__)
#  define UNUSED __attribute__((unused))
#else
#  define UNUSED
#endif

struct editline;
typedef struct editline EditLine;

typedef int (*el_rfunc_t)(EditLine *, wchar_t *);

#define EL_PROMPT 0
#define H_SETSIZE 1
#define EL_EDITOR 2
#define EL_SIGNAL 3
#define H_FIRST   3
#define EL_BIND   4
#define H_LAST    4
#define H_PREV    5
#define H_NEXT    6
#define H_CURR    8
#define EL_ADDFN  9
#define EL_HIST   10
#define H_END     12
#define EL_GETCFN 13
#define EL_REFRESH 20

#define DLSYM_FN(h, var, name) *(void **)&(var) = dlsym((h), (name))
static EditLine *(*libedit_el_init)(const char *, FILE *, FILE *, FILE *);
static int (*libedit_el_set)(EditLine *, int, ...);
static const char *(*libedit_el_gets)(EditLine *, int *);
static unsigned char (*libedit_el_sh_complete)(EditLine *, int);
static int (*libedit_el_resize)(EditLine *);

#ifdef STATICLIBEDIT
static UNUSED int
load_libedit(void)
{
  libedit_el_init = el_init;
  libedit_el_set = el_set;
  libedit_el_gets = el_gets;
  libedit_el_sh_complete = _el_fn_sh_complete;
  libedit_el_resize = el_resize;
  return 1;
}
#else
typedef struct {
  int num;
  const char *str;
} HistEvent;

static UNUSED int
load_libedit(void)
{
  void *h = dlopen("libedit.so.0", RTLD_LAZY | RTLD_LOCAL);
  if (!h)
    return 0;
  DLSYM_FN(h, libedit_el_init, "el_init");
  DLSYM_FN(h, libedit_el_set, "el_set");
  DLSYM_FN(h, libedit_el_gets, "el_gets");
  DLSYM_FN(h, libedit_el_sh_complete, "_el_fn_sh_complete");
  DLSYM_FN(h, libedit_el_resize, "el_resize");
  return libedit_el_init && libedit_el_set && libedit_el_gets && libedit_el_resize;
}
#endif /* STATICLIBEDIT */

#endif /* HISTEDITSHM_H */
