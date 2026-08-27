#ifdef __linux__
  #define _POSIX_C_SOURCE 200809L
#endif /* __linux__ */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>

#include "alloc.h"
#include "main.h"
#include "env.h"
#include "errmsg.h"
#include "pipe.h"
#include "sig.h"

fakestate *fkstate = NULL;
int fakectx = 0;

/* runs after each stage. restoring values to the original */
void
fkrestore(fakestate *fs)
{
  fakectx = LOOPBREAK = LOOPCONT = RETNOW = RETVAL = 0;

  if (fs->cwd >= 0) {
    fchdir(fs->cwd);
    close(fs->cwd);
    fs->cwd = -1;
  }
  if (fs->optsv) {
    SHOPTS = fs->opts;
    fs->opts = fs->optsv = 0;
  }
  if (fs->varc > 0) {
    for (size_t i = 0; i < fs->varc; i++) {
      (fs->vars[i].set) ?
        setvar(fs->vars[i].name, fs->vars[i].val, fs->vars[i].oldflags) : rmvar(fs->vars[i].name);
      sfree(fs->vars[i].name);
      sfree(fs->vars[i].val);
    }
    sfree(fs->vars);
    fs->varc = 0;
    fs->varcap = 0;
  }
  if (fs->pparamsv) {
    freeshargv();
    SHARGV = fs->argv;
    SHARGC = fs->argc;
    SHARGV0 = fs->argv0;
    ALLOCED = fs->argvalloc;
    fs->argv0 = NULL;
    fs->argv = NULL;
    fs->argc = 0;
    fs->argvalloc = 0;
    fs->pparamsv = 0;
  }

  if (fs->umasksv) {
    umask(fs->umask);
    fs->umasksv = 0;
  }

  if (fs->trapsv) {
    unsigned long bits = fs->trapsv;
    while (bits) {
      int i = __builtin_ctzll(bits);
      int ch = !(trap[i] && fs->trap[i] && !strcmp(trap[i], fs->trap[i]));
      sfree(trap[i]);
      trap[i] = fs->trap[i];
      fs->trap[i] = NULL;
      if (ch)
        setsignal(i);
      bits &= bits - 1;
    }
    trapm = fs->trapm;
    fs->trapm = 0;
    fs->trapsv = 0;
  }

  if (fs->rlimsv) {
    unsigned long bits = fs->rlimsv;
    while (bits) {
      int r = __builtin_ctzll(bits);
      struct rlimit cur;
      getrlimit(r, &cur);
      if (cur.rlim_cur != fs->rlim[r].cur || cur.rlim_max != fs->rlim[r].max) {
        cur.rlim_cur = fs->rlim[r].cur;
        cur.rlim_max = fs->rlim[r].max;
        setrlimit(r, &cur);
      }
      bits &= bits - 1;
    }
    fs->rlimsv = 0;
  }
}

/* runs before each stage. makes sure things are cleared */
void
fkinit(fakestate *fs)
{
  fakectx = 1;
  if (fs->cwd >= 0)
    if (close(fs->cwd) < 0)
      sherrx(1, "open");
  if (fs->varc) {
    fs->varc = 0;
    fs->varcap = 0;
    fs->vars = NULL;
  }
  if (fs->optsv) {
    fs->opts = 0;
    fs->optsv = 0;
  }
  if (fs->pparamsv) {
    fs->argc = 0;
    fs->argv0 = NULL;
    fs->argv = NULL;
  }

  if (fs->trapsv) {
    fs->trap = NULL;
    fs->trapsv = 0;
    fs->trapm = 0;
  }
  if (fs->rlimsv) {
    fs->rlim = NULL;
    fs->rlimsv = 0;
  }
  if (fs->aliasc) {
    fs->alias = NULL;
    fs->aliasc = 0;
    fs->aliascap = 0;
  }
  if (fs->umasksv) {
    fs->umask = 0;
    fs->umasksv = 0;
  }
}

void
svfkcwd(fakestate *fs)
{
  if (fs->cwd >= 0)
    return;
  fs->cwd = open(".", O_RDONLY | O_CLOEXEC);
}

void
svfkvar(fakestate *fs, const char *name)
{
  for (size_t i = 0; i < fs->varc; i++)
    if (!strcmp(fs->vars[i].name, name))
      return;

  size_t nlen;
  shvar *v;

  if (!fs->varcap) {
    fs->varcap = 8;
    fs->vars = salloc(fs->varcap * sizeof(tmp_var));
  } else if (fs->varc >= (size_t)fs->varcap) {
    fs->varcap *= 2;
    fs->vars = srealloc(fs->vars, fs->varcap * sizeof(tmp_var));
  }
  nlen = strlen(name);
  if ((v = findvar_n(name, nlen))) {
    fs->vars[fs->varc].val = strndup_(shvar_val(v), vallen(v));
    fs->vars[fs->varc].set = 1;
    fs->vars[fs->varc].oldflags = v->flags;
  } else {
    fs->vars[fs->varc].val = NULL;
    fs->vars[fs->varc].set = 0;
    fs->vars[fs->varc].oldflags = 0;
  }
  fs->vars[fs->varc++].name = strndup_(name, nlen);
}

void
svfkalias(fakestate *fs, const char *name)
{
  for (size_t i = 0; i < fs->aliasc; i++)
    if (!strcmp(fs->alias[i].name, name))
      return;
  size_t nlen = 0;
  alias *a;
  if (!fs->aliascap) {
    fs->aliascap = 8;
    fs->alias = salloc(fs->aliascap * sizeof(tmp_var));
  } else if (fs->aliasc >= (size_t)fs->aliascap) {
    fs->aliascap *= 2;
    fs->alias = srealloc(fs->alias, fs->aliascap * sizeof(tmp_var));
  }
  nlen = strlen(name);
  if ((a = findalias(name))) {
    fs->alias[fs->aliasc].val = strdup_(a->value);
    fs->alias[fs->aliasc].set = 1;
  } else {
    fs->alias[fs->aliasc].val = NULL;
    fs->alias[fs->aliasc].set = 0;
  }
  fs->alias[fs->aliasc++].name = strndup_(name, nlen);
}

void
svfkopts(fakestate *fs)
{
  if (fs->optsv)
    return;
  fs->opts = SHOPTS;
  fs->optsv = 1;
}

void
svfkargv(fakestate *fs)
{
  if (fs->pparamsv)
    return;

  fs->argv0 = strdup_(SHARGV0);
  fs->argc = SHARGC;
  fs->argvalloc = ALLOCED;
  if (SHARGC > 0 && *SHARGV) {
    fs->argv = salloc((SHARGC + 1) * sizeof(char *));
    for (int i = 0; i < SHARGC; i++)
      fs->argv[i] = strdup_(SHARGV[i]);
    fs->argv[SHARGC] = NULL;
  }
  fs->pparamsv = 1;
}

void
svfktraps(fakestate *fs, int sig)
{
  if (fs->trapsv & (1UL << sig))
    return;
  if (!fs->trap)
    fs->trap = salloc(NSIG * sizeof(char *));
  fs->trap[sig] = trap[sig] ? strdup_(trap[sig]) : NULL;
  fs->trapm |= trapm & (1UL << sig);
  fs->trapsv |= 1UL << sig;
}

void
savefkulimit(fakestate *fs, int resrc, u64 cur, u64 max)
{
  if (fs->rlimsv & (1UL << resrc))
    return;
  if (!fs->rlim)
    fs->rlim = salloc(NRLIM * sizeof(fkrlimit));
  fs->rlim[resrc].cur = cur;
  fs->rlim[resrc].max = max;
  fs->rlimsv |= 1UL << resrc;
}

void
svfkumask(fakestate *fs)
{
  if (fs->umasksv)
    return;
  fs->umask = umask(0);
  fs->umask = umask(fs->umask);
  fs->umasksv = 1;
}

