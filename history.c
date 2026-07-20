#define _POSIX_C_SOURCE 200809L

#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#ifdef LIBEDIT
  #include "histeditshm.h"
#endif

#include "alloc.h"
#include "arg.h"
#include "error.h"
#include "history.h"
#include "simpsh.h"
#include "utils.h"
#include "var.h"

#define DIRPERMS (S_IRWXU | S_IRGRP | S_IWGRP | S_IXGRP | S_IROTH | S_IWOTH | S_IXOTH)
#define HISTLAST (histcnt - 1)
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

  if ((hf = findvar_n(STR("HISTFILE"), sizeof("HISTFILE") - 1))) {
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

static char*
flattenhist(const char *line)
{
  size_t len;
  int nl = 0, squote = 0, dquote = 0;

  len = strlen(line);
  while (len > 0 && line[len - 1] == '\n')
    len--;

  if (len == 0)
    return strdup_("");

  for (size_t i = 0; i < len; i++) {
    if (line[i] == '\'' && !dquote)
      squote = !squote;
    else if (line[i] == '"' && !squote)
      dquote = !dquote;
    else if (line[i] == '\n')
      nl++;
  }

  if (nl == 0)
    return strndup_(line, len);

  char *flat, *d;

  flat = salloc(len + nl + 1);
  d = flat;
  squote = dquote = 0;

  for (size_t i = 0; i < len; i++) {
    unsigned char c = line[i];
    if (c == '\'' && !dquote) {
      squote = !squote;
      *d++ = c;
    } else if (c == '"' && !squote) {
      dquote = !dquote;
      *d++ = c;
    } else if (c == '\n') {
      if (squote || dquote) {
        *d++ = ' ';
      } else {
        size_t j = i;
        while (j > 0 && (line[j - 1] == ' ' || line[j - 1] == '\t'))
          j--;
        if (j > 1 && line[j - 1] == '&' && line[j - 2] == '&')
          *d++ = ' ';
        else if (j > 0 && (line[j - 1] == '|' || line[j - 1] == '\\'))
          *d++ = ' ';
        else
          *d++ = ';', *d++ = ' ';
      }
    } else {
      *d++ = c;
    }
  }
  *d = '\0';
  return flat;
}

void
hist_add(const char *cmd)
{
  if (!cmd || !cmd[0])
    return;

  if (histcnt < histsize) {
    shhistory[histcnt++] = flattenhist(cmd);
  } else {
    slfree(shhistory[0]);
    memmove(shhistory, shhistory + 1, (histsize - 1) * sizeof(char *));
    shhistory[histsize - 1] = flattenhist(cmd);
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
   fputs(shhistory[HISTLAST], histfd);
   fputc('\n', histfd);
   fclose(histfd);
}

static char *
gethistline(FILE *f)
{
  size_t cap = 128, n = 0;
  unsigned char *buf;
  buf= salloc(cap);
  int c;

  while ((c = fgetc(f)) != EOF &&  c != '\n') {
    if (n + 1 >= cap)
      buf = srealloc(buf, cap *= 2);
    buf[n++] = (unsigned char)c;
  }

  if (!n && c == EOF) {
    slfree(buf);
    return NULL;
  }
  buf[n] = '\0';
  return (char *)buf;
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

#ifdef LIBEDIT
static int histpos;

int
hist_cb(void *cookie, HistEvent *ev, int op, ...)
{
  (void)cookie;
  switch (op) {
    case H_FIRST:
      if (!histcnt)
        return -1;
      histpos = HISTLAST;  // start at newest
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
      if (histpos >= HISTLAST)
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
#endif /* ifdef LIBEDIT */

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

int
histentry(const char *arg)
{
  int n = 0;
  if (*arg == '-' && isdigit_(arg[1])) {
    while (isdigit_(*arg))
      n = n * 10 + (*arg++ - '0');
    if ((histcnt - n) > 0) {
      return histcnt - n;
    }
    return -1;
  } else if (isdigit_(*arg)) {
    while (isdigit_(*arg))
      n = n * 10 + (*arg++ - '0');
    int b = histnum - histcnt;
    if (n >= b && n < histnum) {
      return n - b;
    }
    return -1;
  } else {
    if (!arg)
      return -1;
    for (int j = histcnt; j >= 0; j--) {
      if (!strcmp(shhistory[j], arg)) {
        return j;
      }
    }
    return -1;
  }
}

int
histexec(char *cmd)
{
  setinputstrn(cmd, strlen(cmd));
  simpsh_run();
  popinput();
  return LSTATUS;
}

int
fccmd(char **argv)
{
  if (!iflag)
    return 1;
  if (!histcnt) {
    shwarn(argv[0], "history is empty");
    return 1;
  }

  int status = 0, flags = 0, argc = 0;
  char *editor = NULL;

  array_len(argv, argc);
  ARGBEGIN
  {
    case 'l':
      flags |= FLAG_l;
      break;
    case 's':
      flags |= FLAG_s;
      break;
    case 'e':
      flags |= FLAG_e;
      if (!(editor = EARGF(no_opt(argv0, ARGC()))))
        return 1;
      break;
    case 'n':
      flags |= FLAG_n;
      break;
    case 'r':
      flags |= FLAG_r;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND

  int first = -1, last = -1, idx;
  char *cmd, *firstarg = NULL, *lastarg = NULL;

  if (*argv) {
    firstarg = *argv++, argc--;
    first = histentry(firstarg);
  }
  if (*argv) {
    lastarg = *argv++, argc--;
    last = histentry(lastarg);
  }

  if (flags & FLAG_l) {
    if (first < 0) {
      if ((first = histcnt - 16) < 0)
        first = 0;
    } else if (first >= histcnt) {
      first = HISTLAST;
    }
    if (last < 0)
      last = HISTLAST;
    else if (last >= histcnt)
      last = HISTLAST;
    if (first > last && !(flags & FLAG_r)) {
      first ^= last;
      last ^= first;
      first ^= last;
    }

    if (flags & FLAG_r) {
      for (int i = last; i >= first; i--) {
        if (flags & FLAG_n) {
          fprintf(stdout, "%s\n", shhistory[i]);
        } else {
          int snum = histnum - histcnt + i;
          fprintf(stdout, "%d\t %s\n", snum, shhistory[i]);
        }
      }
    } else {
      for (int i = first; i <= last; i++) {
        if (flags & FLAG_n) {
          fprintf(stdout, "%s\n", shhistory[i]);
        } else {
          int snum = histnum - histcnt + i;
          fprintf(stdout, "%d\t %s\n", snum, shhistory[i]);
        }
      }
    }
    return 0;
  }

  if (!flags || (flags & FLAG_e)) {
    int fd, err = 0, wstatus;
    shvar *fced, *ed;
    FILE *tmp;
    pid_t pid;

    if (first < 0) {
      if ((first = HISTLAST) < 0)
        first = 0;
    } else if (first >= histcnt) {
      first = HISTLAST;
    }
    if (last < 0)
      last = first;
    else if (last >= histcnt)
      last = HISTLAST;

    if (!(flags & FLAG_e)) {
      if ((fced = findvar_n(STR("FCEDIT"), sizeof("FCEDIT") - 1))) {
        editor = shvar_val(fced);
      } else if ((ed = findvar_n(STR("EDITOR"), sizeof("EDITOR") - 1))) {
        editor = shvar_val(ed);
      } else {
        editor = "ed";
      }
    }

    char tmpname[] = "/tmp/sh-fc.XXXXXX";
    if ((fd = mkstemp(tmpname)) < 0)
      return sherr(1, argv0, "mkstemp");
    if (!(tmp = fdopen(fd, "w+")))
      return sherr(1, argv0, "open");

    char *const editargs[] = {editor, tmpname, NULL};

    if (flags & FLAG_r)
      for (int i = last; i >= first; i--) {
        fputs(shhistory[i], tmp);
        fputc('\n', tmp);
      }
    else
      for (int i = first; i <= last; i++) {
        fputs(shhistory[i], tmp);
        fputc('\n', tmp);
      }

    fflush(tmp);
    switch (pid = fork()) {
      case -1:
        err = 1;
        goto cleanup;
      case 0:
        execvp(editor, editargs);
        _exit(1);
      default:
        waitpid(pid, &wstatus, 0);
    }

    lseek(fd, 0, SEEK_SET);
    setinputf(fd, tmpname, 0);
    simpsh_run();
    popinput();

cleanup:
    fclose(tmp);
    unlink(tmpname);
    if (err)
      return sherr(1, argv0, "fork");
    return LSTATUS;
  }

  if (flags & FLAG_s) {
    int suballoc = 0;
    if (firstarg) {
      if (strchr(firstarg, '=')) {
        char *p, *r;
        st_read_assn(firstarg, &p, &r);
        idx = lastarg ? histentry(lastarg) : HISTLAST;
        if (idx < 0)
          return 1;
        cmd = shhistory[idx];
        if (p && r) {
          char *sub, *post, *new;
          size_t prelen, postlen, rlen;
          if ((sub = strstr(cmd, p))) {
            prelen = sub - cmd;
            post = sub + strlen(p);
            postlen = strlen(post);
            rlen = strlen(r);
            new = salloc(prelen + rlen + postlen + 1);
            suballoc = 1;
            memcpy(new, cmd, prelen);
            memcpy(new + prelen, r, strlen(r));
            memcpy(new + prelen + rlen, post, postlen + 1);
            cmd = new;
          }
        }
      } else {
        idx = firstarg ? histentry(firstarg) : HISTLAST;
        if (idx < 0)
          return 1;
        cmd = shhistory[idx];
      }
    } else {
      cmd = shhistory[HISTLAST];
    }
    status = histexec(cmd);
    if (suballoc) {
      slfree(cmd);
    }
    return status;
  }

  printf("%d", flags);
  return 0;
}
