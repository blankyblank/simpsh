/* simpsh.c - functions for running the shell */
#define _POSIX_C_SOURCE 200809L

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef READLINE
#include <readline/history.h>
#include <readline/readline.h>
#else
#include "utils.h"
#endif /* ifdef READLINE */

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

static char *nxtline;
#ifdef READLINE
#define DIRPERMS S_IRWXU|S_IRGRP|S_IWGRP|S_IXGRP|S_IROTH|S_IWOTH|S_IXOTH
static int pmkdir(char *);
#endif /* ifdef READLINE */

#ifndef MUSL
void
getbuildinfo(void) {
  printf("%s build info:\n"
         "build date: %s %s\n"
         "ansi C standard conformance: %ld\n",
         sh_argv0, __DATE__, __TIME__, __STDC_ISO_10646__);
}
#endif /* ifndef MUSL */

static inline void
stdin_cb(void *data)
{
  (void)data;
#ifdef READLINE
  rl_callback_read_char();
#else
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
#endif /* READLINE */
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

static int
read_cmd(char **restrict cmd, size_t *restrict len)
{
  char *ps1 = expand_ps1(getvar(ps1n));
  char *line = lineread(ps1 ? ps1 : " $ ");
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
  size_t lineslen = n + 1;
#ifdef READLINE
  free(line);
#endif

  stmark mark = stack_mark();
  while (need_more(lines, lineslen)) {
    stack_restore(mark);
    char *ps2 = expand_ps1(getvar(ps2n));
    line = lineread(ps2 ? ps2 : "> ");
    if (!line)
      return -1;
    if (vflag) {
      fputs(line, stderr);
      fputc('\n', stderr);
    }

    n = strlen(line);
    char *new = st_alloc(lineslen + n + 2);
    memcpy(new, lines, lineslen);
    memcpy(new + lineslen, line, n);
    new[lineslen + n] = '\n';
    new[lineslen + n + 1] = '\0';
#ifdef READLINE
    free(line);
#endif
    lines = new;
    lineslen += n + 1;
  }

  *cmd = lines;
  *len = lineslen;
  return 1;
}

int
sh_interactive(void)
{
  char *lines;
  stmark mark;
  size_t lineslen, cpylen;
  int r;
  static shinput *inpt;

#ifdef READLINE
  init_history();
  FILE *tty = fopen("/dev/tty", "w+");
  if (tty) {
    setbuf(tty, NULL);
    rl_outstream = tty;
  }
#endif

  inpt = st_alloc(sizeof(shinput));
  inpt->buf = st_alloc(BUFSIZ);
  inpt->fd = -1;
  inpt->nchar = inpt->buf;
  inpt->prev = shinpt;
  shinpt = inpt;

  for (;;) {
    ttyreclaim();
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

    r = read_cmd(&lines, &lineslen);
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

    cpylen = lineslen > BUFSIZ ? BUFSIZ : lineslen;
    memcpy(inpt->buf, lines, cpylen);
    inpt->nchar = inpt->buf;
    inpt->nleft = cpylen;
    inpt->unget = 0;
    simpsh_run();
#ifdef READLINE
    append_history(1, histfile);
#endif
    stack_restore(mark);  /* frees lines/parse trees, NOT persist */
  }
  ttyrestore();
  return lstatus;
}

#ifdef READLINE

static inline void
handle_line(char *line)
{
  nxtline = line;
  rl_callback_handler_remove();
  stopeventloop(&el);
}

/** read line from interactive shell */
char *
lineread(char *prompt)
{
  addeventloop(&el, STDIN_FILENO, POLLIN, stdin_cb, NULL);
  rl_already_prompted = 1;
  rl_callback_handler_install(prompt, handle_line);
  rl_redisplay();
  nxtline = NULL;
  el.running = 1;
  while (el.running) {
    runeventloop(&el, -1);
    if (intsig) {
      intsig = 0;
      putchar('\n');
      rl_free_line_state();
      rl_cleanup_after_signal();
      rl_already_prompted = 1;
      rl_callback_handler_install(prompt, handle_line);
      rl_redisplay();
      nxtline = NULL;
      el.running = 1;
    }
  }
  rmeventloop(&el, STDIN_FILENO);
  if (nxtline && *nxtline)
    add_history(nxtline);
  return nxtline;
}


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

/** initialize history */
void
init_history(void)
{
  char buf[PATH_MAX];
  snprintf(histfile, PATH_MAX, "%s/.local/state/simpsh/simpsh_history", home);
  using_history();
  if (access(histfile, W_OK) < 0) {
    static const char *histdir = ".local/state/simpsh";
    snprintf(buf, PATH_MAX, "%s/%s", home, histdir);
    if (!pmkdir(buf))
      return;
    if (access(histfile, W_OK) < 0)
      if (write_history(histfile) != 0)
        return;
  } else {
    history_truncate_file(histfile, HISTORY_SIZE);
    if (read_history_range(histfile, 0, -1) != 0)
      perror("Failed to read .simpsh_history");
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
#else

/** lineread with no readline, for testing */
char *
lineread(char *prompt)
{
  addeventloop(&el, STDIN_FILENO, POLLIN, stdin_cb, NULL);
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
#endif /* ifdef READLINE */

