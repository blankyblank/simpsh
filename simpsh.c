/* simpsh.c - functions for running the shell */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef LIBEDIT
  #include <dlfcn.h>
  #include "histeditshm.h"
#else
  #include "utils.h"
#endif /* ifdef LIBEDIT */

#include "alloc.h"
#include "error.h"
#include "exec.h"
#include "expand.h"
#include "history.h"
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
static char *nxtline;

#ifdef LIBEDIT
#define DLSYM_FN(h, var, name) *(void **)&(var) = dlsym((h), (name))
static EditLine *edl;
static int ps1mode;
static EditLine *edl;
static EditLine *(*libedit_el_init)(const char *, FILE *, FILE *, FILE *);
static int (*libedit_el_set)(EditLine *, int, ...);
static const char *(*libedit_el_gets)(EditLine *, int *);
static unsigned char (*libedit_el_sh_complete)(EditLine *, int);

static int input_notify(EditLine *, wchar_t *);
static char *prompt_fn(EditLine *);

  #ifdef STATICLIBEDIT
  static int
  load_libedit(void)
  {
    libedit_el_init = el_init;
    libedit_el_set = el_set;
    libedit_el_gets = el_gets;
    libedit_el_sh_complete = _el_fn_sh_complete;
    return 1;
  }
  #else

  static int
  load_libedit(void)
  {
    void *h = dlopen("libedit.so.0", RTLD_LAZY | RTLD_LOCAL);
    if (!h)
      return 0;
    DLSYM_FN(h, libedit_el_init, "el_init");
    DLSYM_FN(h, libedit_el_set, "el_set");
    DLSYM_FN(h, libedit_el_gets, "el_gets");
    DLSYM_FN(h, libedit_el_sh_complete, "_el_fn_sh_complete");
    return libedit_el_init && libedit_el_set && libedit_el_gets;
  }
  #endif /* STATICLIBEDIT */
#endif   /* LIBEDIT */

static void stdin_cb(void *data);

static inline void
source_file(const char *path)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0)
    return;
  setinputf(fd, path, 0);
  eval_run();
  RETNOW = 0;
  popinput();
}

static char *
update_prompt(i32 ps1)
{
  char *p;

  if (!(p = getvar(ps1 ? STR("PS1") : STR("PS2"))))
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
  while (need_more(*lines, *llen)) {
    line = lineread(0);
    if (!line)
      return -1;
    if (vflag) {
      fputs(line, stderr);
      fputc('\n', stderr);
    }

    n = strlen(line);
    char *new = st_alloc(*llen + n + 2);
    memcpy(new, *lines, *llen);
    memcpy(new + *llen, line, n);
    new[*llen + n] = '\n';
    new[*llen + n + 1] = '\0';
    *lines = new;
    *llen += n + 1;
  }
  return 0;
}

static int
read_cmd(char ** restrict cmd, size_t * restrict len)
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
    if (gvar.home) {
      size_t hl, flen;
      const size_t plen = 8;
      char hprof[PATH_MAX];
      hl = strlen(gvar.home);
      flen = hl + plen + 2;
      memcpy(hprof, gvar.home, hl);
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
    if ((e = findvar_n(STR("ENV"), 3))) {
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
    if (RETNOW) {
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
    if (RETNOW) {
      RETNOW = 0;
      stack_restore(mark);
      break;
    }
    stack_restore(mark);
    last_tok = SHTOK(TNONE);
    if (fchksig)
      dotrap();
    if (iflag || mflag)
      runeventloop(&el, 0);
    if (mflag) {
      killjob();
      if (ndnotify) {
        ndnotify = 0;
        jobnotify();
      }
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
  if (load_libedit()) {
    edl = libedit_el_init(SHARGV0, stdin, stdout, stderr);
    libedit_el_set(edl, EL_EDITOR, "vi");
    libedit_el_set(edl, EL_SIGNAL, 1);
    libedit_el_set(edl, EL_GETCFN, input_notify);
    libedit_el_set(edl, EL_PROMPT, prompt_fn);
    libedit_el_set(
      edl, EL_ADDFN, "sh-complete", "Shell Completion", libedit_el_sh_complete);
    libedit_el_set(edl, EL_BIND, "^I", "sh-complete", NULL);
    static int hcookie;
    libedit_el_set(edl, EL_HIST, hist_cb, &hcookie);
  }
#endif
  init_history();
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
    hist_add(lines);
    hist_save();
    feed_input(inpt, lines, llen);
    simpsh_run();
    stack_restore(mark);
  }
  ttyrestore();
  return LSTATUS;
}

static int
need_more(const char *lines, size_t llen)
{
  typedef enum {
    NCTX_NORMAL,
    NCTX_SQUOTE,
    NCTX_DQUOTE,
    NCTX_CSUB,
    NCTX_ARITH,
    NCTX_BTICK
  } nctx;
  nctx ctx = NCTX_NORMAL;
  int depth = 0;
  int last = 0, prev = 0;
  size_t i;

  for (i = 0; i < llen; i++) {
    int c = (unsigned char)lines[i];
    int next = (i + 1 < llen) ? (unsigned char)lines[i + 1] : 0;

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
        if (c == '$' && next == '(') {
          i++;
          if (i + 1 < llen && lines[i + 1] == '(') {
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
        if (c == '(') {
          depth++;
        } else if (c == ')') {
          if (--depth <= 0)
            ctx = NCTX_NORMAL;
        } else if (c == '$' && next == '(') {
          i++;
          depth++;
          if (i + 1 < llen && lines[i + 1] == '(') {
            i++;
            depth++;
          }
        }
        break;
      case NCTX_ARITH:
        if (c == '(') {
          depth++;
        } else if (c == ')') {
          if (depth > 0) {
            depth--;
          } else if (next == ')') {
            i++;
            ctx = NCTX_NORMAL;
          }
        } else if (c == '$' && next == '(') {
          i++;
          depth++;
          if (i + 1 < llen && lines[i + 1] == '(') {
            i++;
            depth++;
          }
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

  if (ctx != NCTX_NORMAL)
    return 1;
  if (last == '|')
    return 1;
  if (last == '&' && prev == '&')
    return 1;
  if (llen >= 2) {
    i = llen - 2;
    while (i > 0 && (lines[i] == ' ' || lines[i] == '\t'))
      i--;
    if (i > 0 && lines[i] == '\\' && lines[i - 1] != '\\')
      return 1;
  }
  {
    int kdepth = 0, ksquote = 0, kdquote = 0, boundary = 1;
    enum {
      K_IF,
      K_FOR,
      K_WHILE,
      K_UNTIL,
      K_CASE,
      K_DO
    } kstack[16];

    for (size_t k = 0; k < llen; k++) {
      unsigned char c = lines[k];
      if (c == '\'' && !kdquote) {
        ksquote = !ksquote;
        continue;
      }
      if (c == '"' && !ksquote) {
        kdquote = !kdquote;
        continue;
      }
      if (ksquote || kdquote)
        continue;

      if (c == '\n' || c == ';' || c == '|') {
        boundary = 1;
        continue;
      }
      if (c == '&' && k + 1 < llen && lines[k + 1] == '&') {
        boundary = 1;
        k++;
        continue;
      }

      if (boundary && c != ' ' && c != '\t') {
        size_t s = k;
        while (k < llen && lines[k] != ' ' && lines[k] != '\t' &&
               lines[k] != '\n' && lines[k] != ';' && lines[k] != '|')
          k++;
        int wlen = k - s, h = kwhash(lines + s, wlen);
        if (kw[h].len == wlen && memcmp(kw[h].word, lines + s, wlen) == 0) {
          switch (kw[h].tok) {
            case TIF:
              kstack[kdepth++] = K_IF;
              break;
            case TFOR:
              kstack[kdepth++] = K_FOR;
              break;
            case TWHILE:
              kstack[kdepth++] = K_WHILE;
              break;
            case TUNTIL:
              kstack[kdepth++] = K_UNTIL;
              break;
            case TCASE:
              kstack[kdepth++] = K_CASE;
              break;
            case TDO:
              kstack[kdepth++] = K_DO;
              break;
            case TFI:
              while (kdepth && kstack[kdepth - 1] != K_IF)
                kdepth--;
              if (kdepth)
                kdepth--;
              break;
            case TDONE:
              while (kdepth && kstack[kdepth - 1] != K_FOR &&
                     kstack[kdepth - 1] != K_WHILE &&
                     kstack[kdepth - 1] != K_UNTIL &&
                     kstack[kdepth - 1] != K_DO)
                kdepth--;
              if (kdepth)
                kdepth--;
              break;
            case TESAC:
              while (kdepth && kstack[kdepth - 1] != K_CASE)
                kdepth--;
              if (kdepth)
                kdepth--;
              break;
            default:
              break;
          }
        }
        boundary = 0;
      }
    }
    if (kdepth)
      return 1;
  }

  return 0;
}

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
  if (!edl) {
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
  int count;
  char *s;
  const char *line;

again:
  ps1mode = ps1;
  line = libedit_el_gets(edl, &count);
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
  return s;
}

#else
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
