#ifndef HISTEDITSHM_H
#define HISTEDITSHM_H

#include <wchar.h>
struct editline;
typedef struct editline EditLine;
typedef struct { int num; const char *str; } HistEvent;

#define EL_PROMPT    0
#define EL_EDITOR    2
#define EL_SIGNAL    3
#define EL_BIND      4
#define EL_ADDFN     9
#define EL_HIST     10
#define EL_GETCFN   13
#define H_SETSIZE   1
#define H_FIRST     3
#define H_LAST      4
#define H_PREV      5
#define H_NEXT      6
#define H_CURR      8
#define H_END      12

typedef int (*el_rfunc_t)(EditLine *, wchar_t *);

#endif /* HISTEDITSHM_H */
