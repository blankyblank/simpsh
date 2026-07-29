#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "alloc.h"
#include "arg.h"
#include "errmsg.h"
#include "main.h"
#include "expand.h"
#include "opts.h"
#include "path.h"
#include "pipe.h"
#include "utils.h"
#include "var.h"

cmdent chash[CHASH_MAX];
unsigned int chashn;

static char *tildepath(const char * restrict, size_t, size_t * restrict);

char *
findchash(const char *n)
{
  for (size_t i = 0; i < chashn; i++)
    if (n[0] == chash[i].name[0] && strcmp(n, chash[i].name) == 0)
      return chash[i].path;
  return NULL;
}

void
setchash(const char *restrict n, const char *restrict p)
{
  for (size_t i = 0; i < chashn; i++)
    if (n[0] == chash[i].name[0] && strcmp(n, chash[i].name) == 0)
      return;

  if (chashn >= CHASH_MAX) {
    sfree(chash[CHASH_MAX - 1].name);
    sfree(chash[CHASH_MAX - 1].path);
    chashn = CHASH_MAX - 1;
  }
    memmove(&chash[1], &chash[0], chashn * sizeof(cmdent));
    chash[0].name = strdup_(n);
    chash[0].path = strdup_(p);
    if (chashn < CHASH_MAX)
      chashn++;
}

void
rmchash(const char *unused)
{
  (void)unused;
  for (size_t i = 0; i < chashn; i++) {
    sfree(chash[i].name);
    sfree(chash[i].path);
  }
  chashn = 0;
}

static char *
tildepath(const char *restrict s, size_t dirlen, size_t *restrict seg)
{
  size_t lseg;
  char *hm;

  lseg = 1;
  while (s[lseg] != '/' && lseg < dirlen)
    lseg++;

  if (lseg == 1 || (lseg == dirlen && dirlen == 1)) {
    hm = getvar("HOME");
  } else {
    static char tildbuf[PATH_MAX];
    nmemcpy(tildbuf, s + 1, lseg - 1);
    hm = homedir(tildbuf);
  }
  *seg = lseg;
  return hm;
}

/** check in path for name stop at the first one with proper permissions if
 * cdmode 1 check for dir */
char *
chkpath(const char *restrict path, const char *restrict name, int mode, unsigned int cdmode)
{
  size_t flen, seg;
  static char buf[PATH_MAX], expbuf[PATH_MAX];
  const char *s;
  char *e;
  struct stat statbuf;

  flen = strlen(name);
  s = path;

  for (e = strchrnul_(s, ':'); e; e = strchrnul_(s, ':')) {
    size_t dirlen, complen;
    char *end, *comp;

    dirlen = e - s;
    comp = (char *)s;
    complen = dirlen;
    if (s[0] == '~') {
      char  *hm;
      seg = 0;
      hm = tildepath(s, dirlen, &seg);
      if (hm) {
        size_t hmlen;
        hmlen = strlen(hm);
        memcpy(expbuf, hm, hmlen);
        if (seg < dirlen)
          memcpy(expbuf + hmlen, s + seg, dirlen - seg);
        comp = expbuf;
        complen = hmlen + (seg < dirlen ? dirlen - seg : 0);
      }
    }

    if (complen + 1 + flen >= PATH_MAX) {
      if (!*e)
        break;
      s = e + 1;
      continue;
    }
    end = mempcpy_(buf, comp, complen);
    *end++ = '/';
    memcpy(end, name, flen + 1);
    if (access(buf, mode) == 0) {
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

  // XXX: consider hashing full paths
  if ((strchr(file, '/')) && access(file, X_OK) == 0)
    return (st_strdup(file));

  if (hflag && (fullpath = findchash(file)))
    return st_strdup(fullpath);

  const char *path = getvar("PATH");
  if (path)
    fullpath = chkpath(path, file, X_OK, 0);
  else
    fullpath = chkpath(defpath, file, X_OK, 0);

  if (!fullpath)
    return NULL;
  if (hflag)
    setchash(file, fullpath);
  return fullpath;
}

  /*
   * INFO:
   *     we collaps any // to a single /, for .. when encountered
   *     we move back a path segment (between slashes /here/)
   *     for . we collapse it like the // case. we get rid of any 
   *     trailing / also we get rid of any . in the beginning
   *     like ./dir 
   *     we are modifying the string in place using res as the result
   *     buffer chars are getting copied to (and overwriting things we
   *     want to get rid of) and src is the pointer we copy from. it moves
   *     up when we need to skip something while res stays in place, or res
   *     moves back when we get rid of a segment.
   */

/**  normalize path to set PWD variable with logical path  */
static char *
pwdpath(char *path) {
  char *res = path, *src = path;

  while (*src) {
    switch (*src) {
      case '/':
        if ((res != path && *(res - 1) == '/') || *(src + 1) == '\0')
          src++;
        else
          *res++ = *src++;
        break;
      case '.':
        if (*(src + 1) && (*(src + 2) == '/' || *(src + 2) == '\0'))
          if (res > path + 1) {
            if (res > path + 1 && *(res - 1) == '/')
              res--;
            while (res > path && *(res - 1) != '/')
              res--;
            if (res > path + 1)
              res--;
            src += 2;
          } else {
            src += 2;
          }
        else if (*(src + 1) == '/')
          src += 2;
        else if (*(src + 1) == '\0')
          src++;
        else
          *res++ = *src++;
        break;
      default:
        *res++ = *src++;
        break;
    }
  }

  *res = '\0';
  return path;
}

int
cdcmd(char **argv)
{
  unsigned int prnt, argc = 0;
  char *bargv0, *dir, *end, *pwdval;
  char flag = '\0', respath[PATH_MAX];
  const char *dest;
  shvar *pwd, *oldpwd, *cdpth;
  size_t destlen = 0;

  prnt = 0;
  array_len(argv, argc);
  bargv0 = argv[0];
  ARGBEGIN
  {
    case 'L':
      flag = FLAG_L;
      break;
    case 'P':
      flag = FLAG_P;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND;
  if (argc > 1)
    return shwarn(bargv0, "Too many arguments"); /*NOLINT*/

  oldpwd = findvar("OLDPWD");
  pwd = findvar("PWD");
  if (pwd)
    pwdval = shvar_val(pwd);
  else
    pwdval = getcwd(respath, PATH_MAX);
  cdpth = findvar("CDPATH");
  if (cdpth) {
    if (*argv && argv[0][0] != '/' &&
        !(argv[0][0] == '-' && argv[0][1] == '\0') &&
        !(argv[0][0] == '.' && argv[0][1] == '\0') &&
        !(argv[0][0] == '.' && argv[0][1] == '/') &&
        !(argv[0][0] == '.' && argv[0][1] == '.')) {
      if ((dest = chkpath(shvar_val(cdpth), *argv, X_OK, 1))) {
        if (!(dest[0] == '.' && dest[1] == '/'))
          prnt = 1;
      } else {
        dest = *argv;
      }
    } else {
      dest = *argv;
    }
  } else {
    dest = *argv;
  }

  if (!dest) {
    dir = gvar.home;
    destlen = gvar.homelen;
  } else if (*dest == '-' && dest[1] == '\0') {
    if (!oldpwd)
      return shwarn(bargv0, "OLDPWD not set"); /*NOLINT*/
    dir = shvar_val(oldpwd);
    destlen = oldpwd->flen;
    prnt = 1;
  } else {
    dir = (char *)dest;
    destlen = strlen(dir);
  }

  if (flag == FLAG_P) {
    if (destlen >= PATH_MAX)
      nts(respath, destlen - 1);
    if (!realpath(dir, respath))
      return sherr(1, bargv0, dir);
    if (fakectx)
      svfkcwd(fkstate);
    if (chdir(respath) < 0)
      return sherr(1, bargv0, dir);
    if (prnt) {
      printf("%s\n", respath);
    }
    if (getcwd(respath, PATH_MAX))
      return 1;
  } else {
    size_t plen, dlen;
    if (fakectx)
      svfkcwd(fkstate);
    if (chdir(dir) < 0)
      return sherr(1, bargv0, dir);
    if (prnt == 1) {
      printf("%s\n", dir);
    }
    dlen = strlen(dir);
    if (dir != NULL && dir[0] == '/') {
      memcpy(respath, dir, dlen);
      respath[dlen] = '\0';
    } else {
      if (!pwdval)
        return 1;
      plen = strlen(pwdval);
      end = mempcpy_(respath, pwdval, plen);
      *end++ = '/';
      end = mempcpy_(end, dir, dlen);
      *end = '\0';
    }
    if (!pwdpath(respath))
      return shwarn(bargv0, "path normalization failure"); /*NOLINT*/
  }
  setvar("OLDPWD", pwdval, VEXPRT);
  setvar("PWD", respath, VEXPRT);
  return 0;
}

int
hashcmd(char **argv)
{
  size_t argc = 0;
  int flags;
  char *bargv0;

  array_len(argv, argc);
  bargv0 = argv[0];
  flags = 0;

  ARGBEGIN
  {
    case 'r':
      flags |= FLAG_r;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND

  if (flags & FLAG_r) {
    rmchash(NULL);
    return 0;
  } else if (!argc) {
    for (size_t i = 0; i < chashn; i++)
      puts(chash[i].path);
    return 0;
  } else {
    const char *path, *fpath;
    if (!(path = getvar("PATH")))
      path = defpath;
    for (size_t i = 0; i < argc; i++) {
      if (!(fpath = chkpath(path, argv[i], X_OK, 0))) {
        shwarn_arg(bargv0, argv[i], "not found");
        return 1;
      }
      setchash(argv[i], fpath);
    }
    return 0;
  }
  return 1;
}

int
pwdcmd(char **argv)
{
  int argc = 0;
  (void)argc;
  char flag = '\0';
  char pwdbuf[PATH_MAX + 1];

  ARGBEGIN
  {
    case 'L':
      flag = FLAG_L;
      break;
    case 'P':
      flag = FLAG_P;
      break;
    default:
      bad_opt(argv0, ARGC());
      return 1;
  }
  ARGEND;

  if (flag != FLAG_P) {
    char *pwd = getvar("PWD");
    struct stat sbuf, cwdsbuf;

    if (!pwd) {
      goto physical;
    }
    if (stat(pwd, &sbuf) < 0) {
      goto physical;
    }
    if (stat(".", &cwdsbuf) < 0) {
      warn("pwd");
      return 1;
    }
    if ((sbuf.st_ino != cwdsbuf.st_ino || sbuf.st_dev != cwdsbuf.st_dev))
      goto physical;

    printf("%s\n", pwd);
    return 0;
  }

physical:
  if (getcwd(pwdbuf, PATH_MAX + 1)) {
    printf("%s\n", pwdbuf);
    return 0;
  } else {
    warn("pwd");
    return 1;
  }
}
