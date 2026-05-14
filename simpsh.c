/* simpsh.c - functions for running the shell */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <linux/limits.h>
#ifdef READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif /* ifdef READLINE */

#include "main.h"
#include "simpsh.h"
#include "utils.h"
#include "env.h"

void
getbuildinfo(void) {
  printf("%s build info:\n"
         "build date: %s %s\n"
         "ansi C standard conformance: %ld\n",
         sh_argv0, __DATE__, __TIME__, __STDC_ISO_10646__);
}

#ifdef READLINE
static int create_histfile(char *, char *);
#endif /* ifdef READLINE */

/** make directory path */
static int pmkdir(char *path);

#ifdef READLINE
/** read line from interactive shell */
char *
lineread(char *prompt)
{
  char *line = readline(prompt);
  if (line && *line)
    add_history(line);

  return line;
}
#else
/** lineread with no readline, for testing */
char *
lineread(char *prompt)
{
  char buf[4096];
  fputs(prompt, stdout);
  if (!fgets(buf, sizeof(buf), stdin))
    return NULL;
  size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\n')
    buf[len - 1] = '\0';
  return s_strdup(buf);
}
#endif /* ifdef READLINE */

#ifdef READLINE
/** create history file */
int
create_histfile(char *home, char *histfile)
{
  static const char *histdir = ".local/state/simpsh";
  char buf[256];

  snprintf(buf, 256, "%s/%s", home, histdir);
  if (!pmkdir(buf))
    return 0;
  if (access(histfile, W_OK) < 0)
    if (write_history(histfile) != 0)
      return 0;
  return 1;
}

/** initialize history */
void
init_history(void)
{
  char histfile[265];
  snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);
  using_history();
  if (access(histfile, W_OK) < 0) {
    if (!create_histfile(home, histfile)) /* create if needed */
      perror("Failed to create simpsh_history:");
  } else {
    history_truncate_file(histfile, 1000);
    if (read_history_range(histfile, 0, -1) != 0)
      perror("Failed to read .simpsh_history");
  }
}
#endif /* ifdef READLINE */

/** take in ps1 char * to do variable expansion for prompt */
char *
expand_ps1(char *p)
{
  size_t i, end, outlen;
  size_t pvarlen, cbrace;
  char *s, *pcpy, *expanded;

  if (!p)
    return NULL;

  pvarlen = 0;
  outlen = 0;
  i = 0;
  while (p[i]) {

    if (p[i] == '$') {
      if (p[i + 1] == '$' || p[i + 1] == '?') {
        pvarlen = 2;
        pcpy = st_strndup(p + i, pvarlen);
      } else if (p[i + 1] == '{') {
        cbrace = i + 2;
        while (p[cbrace]) {
          if (p[cbrace] == '}') {
            pvarlen = cbrace - i + 1;
            break;
          }
          cbrace++;
        }
        if (!p[cbrace]) {
          st_putc(p[i]);
          outlen++;
          i++;
          continue;
        } else {
          pcpy = st_strndup(p + i, pvarlen);
        }
      } else if (isalnum(p[i + 1])) {
        pvarlen = i + 1;
        while (isalnum(p[pvarlen]))
          pvarlen++;
        pvarlen -= i;
        pcpy = st_strndup(p + i, pvarlen);
      } else {
        if (p[i + 1] == ' ' || p[i + 1] == '\t' || p[i + 1] == '\0') {
          if (lstatus == 0)
            st_putc('$');
          else
            st_putc('X');
          outlen++;
          i++;
          continue;
        }
        st_putc(p[i]);
        outlen++;
        i++;
        continue;
      }
      if ((expanded = exp_var(pcpy, 0, &end))) {
        s = expanded;
        for (; *s; s++) {
          st_putc(*s);
          outlen++;
        }
        i += pvarlen;
      } else {
        for (s = pcpy; *s; s++) {
          st_putc(*s);
          outlen++;
        }
        i += pvarlen;
      }
    } else {
      st_putc(p[i]);
      outlen++;
      i++;
    }
  }
  return grab_str(outlen);
}

/** check in path for name stop at the first one with proper permissions if cdmode 1 check for dir */
char *
chkpath(const char *path, const char *name, unsigned int cdmode)
{
  size_t flen, seg, tlen, blen;
  char *end, *e, *exp, *bdir;
  char buf[PATH_MAX], expbuf[PATH_MAX], tildbuf[PATH_MAX];
  unsigned int noex;
  const char *s;
  struct stat statbuf;

  flen = strlen(name);
  s = path;

  for (e = s_strchrnul(s, ':'); e; e = s_strchrnul(s, ':')) {
    size_t dirlen, complen;
    char *comp;

    dirlen = e - s;
    comp = (char *)s;
    complen = dirlen;

    noex = 0;
    if (s[0] == '~') {
      seg = 0;
      while (s[seg] != '/' && seg < dirlen)
        seg++;

      if (seg == 1 || (seg == dirlen && dirlen == 1)) {
        exp = getvar("HOME");
        if (exp) {
          blen = strlen(exp);
          bdir = exp;
        } else {
          noex = 1;
        }
      } else {
        nmemcpy(tildbuf, s + 1, seg - 1);
        exp = homedir(tildbuf);
        if (exp) {
          blen = strlen(exp);
          bdir = exp;
        } else {
          noex = 1;
        }
      }
      if (!noex) {
        memcpy(expbuf, bdir, blen);
        if (seg < dirlen)
          memcpy(expbuf + blen, s + seg, dirlen - seg);
        tlen = blen + (seg < dirlen ? dirlen - seg : 0);
        comp = expbuf;
        complen = tlen;
      }
    }

    end = s_mempcpy(buf, comp, complen); /* copy current path segment from s to e */
    *end++ = '/';                                /* add / and then file to the end of it */
    memcpy(end, name, flen + 1);
    if (access(buf, X_OK) == 0) {
      if (cdmode) {
        if (!stat(buf, &statbuf) && S_ISDIR(statbuf.st_mode))
          return st_strdup(buf);
      } else {
        return st_strdup(buf);
      }
    }
    if (!*e)
      break;
    s = e + 1;
  }
  return NULL;
}

/** get full path to executable */
char *
getpath(char *file)
{
  char *fullpath;

  if ((strchr(file, '/')) && access(file, X_OK) == 0)
    return (st_strdup(file));

  const char *path = getvar("PATH");
  if (path)
    fullpath = chkpath(path, file, 0);
  else
    fullpath = chkpath(defpath, file, 0);

  if (!fullpath)
    return NULL;
  return fullpath;
}

/**  created directories and their parents if they don't exist  */
static int
pmkdir(char *path)
{
  char *dir; /* clang-format off */

  dir = strchr(path +1, '/');
  while (dir) {
    *dir = 0;
    if (access(path, F_OK) < 0 && mkdir(path, S_IRWXU|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) < 0)
      return 0;
    *dir = '/';
    dir = strchr(dir+1, '/');
  }
  if (mkdir(path, S_IRWXU|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH) < 0)
    return 0;
  return 1;
  /* clang-format on */
}
