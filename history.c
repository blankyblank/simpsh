#define _POSIX_C_SOURCE 200809L

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef LIBEDIT
  #include <histedit.h>
#endif

#include "alloc.h"
#include "history.h"
#include "utils.h"
#include "var.h"

#define DIRPERMS (S_IRWXU | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)
static int pmkdir(char *);

char **shhistory = NULL;
char *histfile;
int histsize;
int histcnt;
int histcur;
int histnum;
#define HISTDIR ".local/state/simpsh"
#define HISTLOC "/.local/state/simpsh/simpsh_history"

/** initialize history */
void
init_history(void)
{
  char buf[PATH_MAX];
  char *histsize_s;
  shvar *hf;

  if ((histsize_s = getvar(STR("HISTSIZE"))))
    histsize = atoi_(histsize_s);
  else
   histsize = HISTORY_SIZE;

  shhistory = slcalloc(histsize + 1, sizeof(char *));
  histcnt = 0;
  histcur = -1;
  histnum = 1;

  if ((hf = findvar_n(STR("HISTFILE"), sizeof("HISTFILE")))) {
    histfile = strdup_(shvar_val(hf));
  } else {
    size_t hflen;
    hflen = gvar.homelen + sizeof(HISTLOC);
    histfile = salloc(hflen + 1);
    snprintf(histfile, hflen + 1, "%s/%s", gvar.home, HISTLOC);

    if (access(histfile, W_OK) < 0) {
      snprintf(buf, PATH_MAX, "%s/%s", gvar.home, HISTDIR);
      if (!pmkdir(buf))
        return;
    }
  }

  hist_load();
}

void
hist_add(const char *cmd)
{
  if (!cmd || !cmd[0])
    return;

  if (histcnt < histsize) {
    shhistory[histcnt++] = strdup_(cmd);
  } else {
    slfree(shhistory[0]);
    memmove(shhistory, shhistory + 1, (histsize - 1) * sizeof(char *));
    shhistory[histsize - 1] = strdup_(cmd);
  }
  histcur = -1;
  histnum++;
  // NOTE: make sure to add numbers to entries in the file.
}

void
hist_save(void)
{
  FILE *histfd;
   if (!(histfd = fopen(histfile, "a"))) {
     warn("%s:failed to open %s", shname, histfile);
     return;
   }

   for (size_t j = 0; shhistory[histcnt - 1][j]; j++) {
     char *line = shhistory[histcnt - 1];
     if (line[j] == '\n') {
       fputc(' ', histfd);
       continue;
     }
     fputc(line[j], histfd);
   }
   fputc('\n', histfd);
   fclose(histfd);
}

static char *
gethistline(FILE *f)
{
  size_t cap = 128, n = 0;
  char *buf = salloc(cap);
  int c;

  while ((c = fgetc(f)) != EOF &&  c != '\n') {
    if (n + 1 >= cap)
      buf = srealloc(buf, cap *= 2);
    buf[n++] = c;
  }

  if (!n && c == EOF) {
    slfree(buf);
    return NULL;
  }
  buf[n] = '\0';
  return buf;
}

void
hist_load(void)
{
  FILE *histfd;
  char *line;

  if (!(histfd = fopen(histfile, "r")))
    return;
  while ((line = gethistline(histfd))) {
    if (line[0])
      hist_add(line);
    slfree(line);
  }
  histnum = histcnt + 1;
  fclose(histfd);
}

void
hist_cleanup(void)
{
  for (int i = 0; i < histcnt; i++)
    slfree(shhistory[i]);
  slfree(shhistory);
  shhistory = NULL;
  histcnt = 0;
  histcur = -1;
}

static int histpos;

int
hist_cb(void *cookie, HistEvent *ev, int op, ...)
{
  (void)cookie;
  switch (op) {
    case H_FIRST:
      if (!histcnt)
        return -1;
      histpos = histcnt - 1;  // start at newest
      ev->str = shhistory[histpos];
      return 0;
    case H_NEXT:
      if (histpos <= 0)
        return -1;  // at oldest, can't go further
      histpos--;
      ev->str = shhistory[histpos];
      return 0;
    case H_LAST:
      if (!histcnt)
        return -1;
      ev->str = shhistory[0];  // oldest
      return 0;
    case H_CURR:
      if (histpos >= 0 && histpos < histcnt) {
        ev->str = shhistory[histpos];
        return 0;
      }
      return -1;
    case H_PREV:
      if (histpos >= histcnt - 1)
        return -1;
      histpos++;
      ev->str = shhistory[histpos];
      return 0;
    case H_END:
    case H_SETSIZE:
      return 0;
    default:
      return -1;
  }
}

/**  created directories and their parents if they don't exist  */
static int
pmkdir(char *path)
{
  char *dir;

  dir = strchr(path + 1, '/');
  while (dir) {
    *dir = 0;
    if (access(path, F_OK) < 0 && mkdir(path, DIRPERMS) < 0)
      return 0;
    *dir = '/';
    dir = strchr(dir + 1, '/');
  }
  if (mkdir(path, DIRPERMS) < 0)
    return 0;
  return 1;
}
