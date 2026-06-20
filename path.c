#include <stddef.h>
#define _POSIX_C_SOURCE 200809L
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arg.h"
#include "expand.h"
#include "path.h"
#include "main.h"
#include "opts.h"
#include "utils.h"
#include "var.h"

cmdent chash[CHASH_MAX];
size_t chashn;

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
    slfree(chash[CHASH_MAX - 1].name);
    slfree(chash[CHASH_MAX - 1].path);
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
    slfree(chash[i].name);
    slfree(chash[i].path);
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

    // BUG: stack overflow find way to resturcture to continue
    //
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
    char *path, *fpath;
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

