#ifndef HISTORY_H
#define HISTORY_H

#ifdef LIBEDIT
  #include "histeditshm.h"
  int hist_cb(void *cookie, HistEvent *ev, int op, ...);
#endif

#define HISTORY_SIZE 1000
extern char *histfile;

extern char **shhistory;
extern int histsize;
extern int histcnt;
extern int histcur;
extern int histnum;

void init_history(void);
void hist_add(const char *);
void hist_save(void);
void hist_load(void);
void hist_cleanup(void);

#endif /* HISTORY_H */
