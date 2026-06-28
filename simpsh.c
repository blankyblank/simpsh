/* simpsh.c - functions for running the shell */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef LIBEDIT
#include <histedit.h>
#else
#include "utils.h"
#endif /* ifdef LIBEDIT */

#include "alloc.h"
#include "error.h"
#include "exec.h"
#include "expand.h"
#include "input.h"
#include "job.h"
#include "lex.h"
#include "main.h"
#include "parse.h"
#include "sig.h"
#include "simpsh.h"
#include "var.h"

static int need_more(const char *, size_t);
static int read_cont(char **, size_t *);
static int read_cmd(char ** restrict, size_t * restrict);
static shinput *init_interactive(void);

#ifdef LIBEDIT
#define DIRPERMS (S_IRWXU|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH)
static EditLine *thel;
static History *hist;
static int ps1mode;

static int pmkdir(char *);
static int input_notify(EditLine *, wchar_t *);
static char *prompt_fn(EditLine *);
void init_history(void);
#else
static char *nxtline;

static void stdin_cb(void *data);
#endif /* LIBEDIT */

static inline void
source_file(const char *path)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return;
  setinputf(fd, 0);
  eval_run();
  retnow = 0;
  popinput();
}

static char *
update_prompt(intf ps1)
{
  char *p;

  if (!(p = getvar(ps1 ? ps1n : ps2n)))
    return ps1 ? " $ " : " > ";
  return expand_ps1(p);
}

static inline void
feed_input(shinput *inpt, const char *lines, size_t llen)
{
  size_t cpylen;
  cpylen = llen > BUFSIZ ? BUFSIZ : llen;
  memcpy(inpt->buf, lines, cpylen);
  inpt->nchar = inpt->buf;
  inpt->nleft = cpylen;
  inpt->unget = 0;
}


static int
read_cont(char **lines, size_t *llen)
{
  char *line;
  size_t n;
  stmark mark = stack_mark();
  while (need_more(*lines, *llen)) {
    stack_restore(mark);
    line = lineread(0);
    if (!line)
      return -1;
    if (vflag) {
      fputs(line, stderr);
      fputc('\n', stderr);
    }

    n = strlen(line);
    char *new = st_alloc(*llen + n + 2);
    memcpy(new, lines, *llen);
    memcpy(new + *llen, line, n);
    new[*llen + n] = '\n';
    new[*llen + n + 1] = '\0';
    *lines = new;
    *llen += n + 1;
  }
  return 0;
}

static int
read_cmd(char **restrict cmd, size_t *restrict len)
{
  char *line = lineread(1);
  if (!line)
    return 0;
  if (vflag) {
    fputs(line, stderr);
    fputc('\n', stderr);
  }
  size_t n = strlen(line);
  char *lines = st_alloc(n + 2);
  memcpy(lines, line, n);
  lines[n] = '\n';
  lines[n + 1] = '\0';
  size_t llen = n + 1;
  if (read_cont(&lines, &llen) < 0)
    return -1;
  *cmd = lines;
  *len = llen;
  return 1;
}

static shinput *
init_interactive(void)
{
  static shinput *inpt;
  inpt = st_alloc(sizeof(shinput));
  inpt->buf = st_alloc(BUFSIZ);
  inpt->fd = -1;
  inpt->nchar = inpt->buf;
  inpt->prev = shinpt;
  inpt->strpush = NULL;
  inpt->b.mapsize = 0;
  inpt->linenum = 1;
  inpt->unget = 0;
  inpt->nleft = 0;
  shinpt = inpt;
  return inpt;
}

void
init_rc(int flag)
{
  if (flag & LOGIN) {
    source_file("/etc/profile");
    if (home) {
      size_t hl, flen;
      const size_t plen = 8;
      char hprof[PATH_MAX];
      hl = strlen(home);
      flen = hl + plen + 2;
      memcpy(hprof, home, hl);
      hprof[hl] = '/';
      memcpy(hprof + hl + 1, ".profile", plen);
      hprof[flen - 1] = '\0';
      source_file(hprof);
    }
  }

  if (iflag && getuid() == geteuid() && getgid() == getegid()) {
    char *f;
    size_t elen;
    shvar *e;
    if ((e = findvar_n(envn, 3))) {
      f = exp_str(shvar_val(e), vallen(e), &elen);
      source_file(f);
    }
  }
}

/* simplified shell loop for eval and '.' */
int
eval_run(void)
{
  int status = 0;
  for (;;) {
    cmd_tree *c;
    stmark mark;

    if (last_tok.type == TNL)
      last_tok = SHTOK(TNONE);
    mark = stack_mark();
    chkwd = CHKKWD | CHKALIAS | CHKNL;
    c = parse_list(TEOF);
    if (!c) {
      stack_restore(mark);
      last_tok = SHTOK(TNONE);
      break;
    }
    if (!nflag)
      status = run_commands(c, 0);
    if (retnow) {
      stack_restore(mark);
      break;
    }
    stack_restore(mark);
    last_tok = SHTOK(TNONE);
  }
  return status;
}

void
simpsh_run(void)
{
  for (;;) {
    cmd_tree *c;
    stmark mark;

    if (last_tok.type == TNL)
      last_tok = SHTOK(TNONE);

    mark = stack_mark();
    chkwd = CHKKWD | CHKALIAS | CHKNL;

    if (fchksig)
      dotrap();
    c = parse_list(TEOF);
    if (!c) {
      stack_restore(mark);
      last_tok = SHTOK(TNONE);
      break;
    }
    if (!nflag)
      run_commands(c, 0);
    fflush(stdout);
    if (retnow) {
      retnow = 0;
      stack_restore(mark);
      break;
    }
    stack_restore(mark);
    last_tok = SHTOK(TNONE);
    if (fchksig)
      dotrap();
    runeventloop(&el, 0);
    killjob();
    if (ndnotify) {
      ndnotify = 0;
      jobnotify();
    }
    if (intsig)
      intsig = 0;
  }
}

int
sh_interactive(void)
{
  char *lines;
  stmark mark;
  shinput *inpt;
  size_t llen;
  int r;

#ifdef LIBEDIT
  HistEvent ev;
  thel = el_init(sh_argv0, stdin, stdout, stderr);
  el_set(thel, EL_EDITOR, "vi");
  el_set(thel, EL_SIGNAL, 1);
  el_set(thel, EL_GETCFN, input_notify);
  el_set(thel, EL_PROMPT, prompt_fn);
  hist = history_init();
  el_set(thel, EL_HIST, history, hist);
  init_history();
#endif

  inpt = init_interactive();

  for (;;) {
    ttyreclaim();
    /* service events between sub-commands within same command list */
    runeventloop(&el, 0);
    killjob();
    if (ndnotify || !bflag) {
      ndnotify = 0;
      jobnotify();
    }
    ndnotify = 0;
    if (intsig) {
      intsig = 0;
      putchar('\n');
    }
    mark = stack_mark();

    r = read_cmd(&lines, &llen);
    if (r == 0) {
      if (Iflag && iflag) {
        clearerr(stdin);
        if ((write(STDOUT_FILENO, dmsg, strlen(dmsg) + 1)) < 0)
          err(1, "write");
        stack_restore(mark);
        continue;
      }
      stack_restore(mark);
      break;
    }
    if (r < 0) {
      stack_restore(mark);
      return 1;
    }

    feed_input(inpt, lines, llen);
    simpsh_run();
#ifdef LIBEDIT
    history(hist, &ev, H_SAVE, histfile);
#endif
    stack_restore(mark);
  }
  ttyrestore();
  return lstatus;
}


static int
need_more(const char *lines, size_t lineslen)
{
  enum { NCTX_NORMAL, NCTX_SQUOTE, NCTX_DQUOTE, NCTX_CSUB,
         NCTX_ARITH, NCTX_BTICK };
  int ctx = NCTX_NORMAL;
  int depth = 0;
  int last = 0, prev = 0;
  size_t i;

  for (i = 0; i < lineslen; i++) {
    int c = (unsigned char)lines[i];
    int next = (i + 1 < lineslen) ? (unsigned char)lines[i + 1] : 0;

    switch (ctx) {
    case NCTX_NORMAL:
      if (c == '\'')
        ctx = NCTX_SQUOTE;
      else if (c == '"')
        ctx = NCTX_DQUOTE;
      else if (c == '`')
        ctx = NCTX_BTICK;
      else if (c == '\\' && next)
        i++;
      else if (c == '$' && next == '(') {
        i++;
        if (i + 1 < lineslen && lines[i + 1] == '(') {
          i++;
          ctx = NCTX_ARITH;
          depth = 0;
        } else {
          ctx = NCTX_CSUB;
          depth = 1;
        }
      } else if (c != '\n' && c != ' ' && c != '\t') {
        prev = last;
        last = c;
      }
      break;
    case NCTX_SQUOTE:
      if (c == '\'')
        ctx = NCTX_NORMAL;
      break;
    case NCTX_DQUOTE:
      if (c == '"')
        ctx = NCTX_NORMAL;
      else if (c == '\\' && next)
        i++;
      break;
    case NCTX_CSUB:
      if (c == '(') depth++;
      else if (c == ')') {
        if (--depth <= 0) ctx = NCTX_NORMAL;
      } else if (c == '$' && next == '(') {
        i++; depth++;
        if (i + 1 < lineslen && lines[i + 1] == '(')
          { i++; depth++; }
      }
      break;
    case NCTX_ARITH:
      if (c == '(') depth++;
      else if (c == ')') {
        if (depth > 0) depth--;
        else if (next == ')') { i++; ctx = NCTX_NORMAL; }
      } else if (c == '$' && next == '(') {
        i++; depth++;
        if (i + 1 < lineslen && lines[i + 1] == '(')
          { i++; depth++; }
      }
      break;
    case NCTX_BTICK:
      if (c == '`')
        ctx = NCTX_NORMAL;
      else if (c == '\\' && next)
        i++;
      break;
    }
  }

  if (ctx != NCTX_NORMAL) return 1;
  if (last == '|') return 1;
  if (last == '&' && prev == '&') return 1;
  if (lineslen >= 2) {
    i = lineslen - 2;
    while (i > 0 && (lines[i] == ' ' || lines[i] == '\t')) i--;
    if (i > 0 && lines[i] == '\\' && lines[i-1] != '\\')
      return 1;
  }
  return 0;
}


#ifdef LIBEDIT
static int
input_notify(EditLine *e, wchar_t *wc)
{
  (void)e;
  struct pollfd fds[2];
  fds[0].fd = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[1].fd = selfpipe[0];
  fds[1].events = POLLIN;

  for (;;) {
    if (poll(fds, 2, -1) < 0)
      return -1;

    if (fds[1].revents & POLLIN) {
      drain_chldp();
      if (bflag && ndnotify) {
        ndnotify = 0;
        jobnotify();
      }
      continue;
    }

    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1)
      return -1;
    *wc = c;
    return 1;
  }
}

static char *
prompt_fn(EditLine *e)
{
  (void)e;
  return update_prompt(ps1mode);
}

/** read line from interactive shell */
char *
lineread(int ps1)
{
  int count;
  char *s;
  const char *line;
  HistEvent ev;

again:
  ps1mode = ps1;
  line = el_gets(thel, &count);
  if (!line) {
    if (!count) {
      putchar('\n');
      return NULL;
    }
    if (errno == EINTR) {
      putchar('\n');
      goto again;
    }
    return NULL;
  }

  if (count > 0 && line[count - 1] == '\n')
    count--;
  s = st_alloc(count + 1);
  memcpy(s, line, count);
  s[count] = '\0';
  history(hist, &ev, H_ENTER, s);
  return s;
}

/** initialize history */
void
init_history(void)
{
  char buf[PATH_MAX];
  HistEvent ev;
  snprintf(histfile, PATH_MAX, "%s/.local/state/simpsh/simpsh_history", home);
  history(hist, &ev, H_SETSIZE, HISTORY_SIZE);
  if (access(histfile, W_OK) < 0) {
    static const char *histdir = ".local/state/simpsh";
    snprintf(buf, PATH_MAX, "%s/%s", home, histdir);
    if (!pmkdir(buf))
      return;
  }
  history(hist, &ev, H_LOAD, histfile);
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
#else

static void
stdin_cb(void *data)
{
  (void)data;
  char buf[4096];
  ssize_t n = read(STDIN_FILENO, buf, sizeof(buf) - 1);
  if (n > 0) {
    char *nl = memchr(buf, '\n', n);
    if (nl)
      *nl = '\0';
    else
      buf[n] = '\0';
    nxtline = st_strdup(buf);
    stopeventloop(&el);
  } else if (!n) {
    stopeventloop(&el);
  }
}

/** lineread with no readline, for testing */
char *
lineread(int ps1)
{
  char *prompt;
  addeventloop(&el, STDIN_FILENO, POLLIN, stdin_cb, NULL);
  prompt = update_prompt(ps1);
  fputs(prompt, stdout);
  fflush(stdout);
  nxtline = NULL;
  el.running = 1;

  while (el.running) { 
    runeventloop(&el, -1);
    if (intsig) {
      intsig = 0;
      putchar('\n');
      fputs(prompt, stdout);
      fflush(stdout);
      nxtline = NULL;
      el.running = 1;
      continue;
    }
  }
  rmeventloop(&el, STDIN_FILENO);
  return nxtline;
}
#endif /* ifdef LIBEDIT */

