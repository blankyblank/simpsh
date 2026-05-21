/* simpsh.c - functions for running the shell */
#define _POSIX_C_SOURCE 200809L
#include <linux/limits.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef READLINE
#include <readline/history.h>
#include <readline/readline.h>
#endif /* ifdef READLINE */

#include "env.h"
#include "exec.h"
#include "input.h"
#include "job.h"
#include "lex.h"
#include "main.h"
#include "malloc.h"
#include "sig.h"
#include "simpsh.h"


#ifdef READLINE
/** make directory path */
static int pmkdir(char *path);
#endif /* ifdef READLINE */
static char *expand_ps1(char *);

void
getbuildinfo(void) {
  printf("%s build info:\n"
         "build date: %s %s\n"
         "ansi C standard conformance: %ld\n",
         sh_argv0, __DATE__, __TIME__, __STDC_ISO_10646__);
}

/** take in ps1 char * to do variable expansion for prompt */
static char *
expand_ps1(char *p)
{
  size_t i, end, varlen, cbrace, flen;
  char *s, *expanded;
  char f[4096], vcpy[256];

  if (!p)
    return NULL;

  varlen = 0;
  flen = 0;
  i = 0;
  while (p[i]) {
    if (p[i] == '$') {
      if (p[i + 1] == '$' || p[i + 1] == '?') {
        varlen = 2;
        vcpy[0] = p[i];
        vcpy[1] = p[i + 1];
        vcpy[2] = '\0';
      } else if (p[i + 1] == '{') {
        cbrace = i + 2;
        while (p[cbrace] && p[cbrace] == '}') {
          cbrace++;
          if (!p[cbrace]) {
            f[flen++] = p[i++];
            continue;
          }
          varlen = cbrace - i + 1;
          memcpy(vcpy, p + i, varlen);
          vcpy[varlen] = '\0';
        }
      } else if (isalnum(p[i + 1])) {
        varlen = i + 1;
        while (isalnum(p[varlen]))
          varlen++;
        varlen -= i;
        memcpy(vcpy, p + i, varlen);
        vcpy[varlen] = '\0';
      } else {
        if (p[i + 1] == ' ' || p[i + 1] == '\t' || p[i + 1] == '\0') {
          f[flen++] = lstatus == 0 ? '$' : 'X';
          i++;
          continue;
        }
        f[flen++] = p[i++];
        continue;
      }
      if ((expanded = exp_var(vcpy, 0, &end))) {
        for (s = expanded; *s; s++)
          f[flen++] = *s;
      }
      i += varlen;
    } else {
      f[flen++] = p[i++];
    }
  }
  f[flen] = '\0';
  s = st_alloc(flen + 1);
  memcpy(s, f, flen + 1);
  return s;
}

void
simpsh_run(void)
{
  for (;;) {
    cmd_tree *c;
    stmark mark;

    if (last_tok.type == TNL)   // ← NEW: consume trailing newline
      last_tok = SHTOK(TNONE);  // from previous command
    
    mark = stack_mark();
    chkwd = CHKALIAS | CHKNL;
    c = parse_list(TEOF);
    if (!c) {
      stack_restore(mark);
      break;
    }
    run_commands(c);
    stack_restore(mark);
  }
}

int
sh_interactive(void)
{
#ifdef READLINE
  init_history();
  FILE *tty = fopen("/dev/tty", "w+");
  if (tty) {
    setbuf(tty, NULL);  // unbuffered for immediate echo
    rl_outstream = tty;
  }
#endif            /* ifdef READLINE */

  char *line, *acc, *new;
  stmark mark, accend, m, base;
  size_t n, acclen;
  int inp;
  sh_tok tmp, lastt;

  /* set up signal handler to handle ctrl-c */
  inp = 0;
  init_sig();
  init_job();

  for (;;) {
    base = stack_mark();
    if (sigsetjmp(shenv, 1)) {
      killjob();
      tcsetpgrp(STDIN_FILENO, sh_pgid);
      if (inp) {
        inp = 0;
        popinput();
      }
      stack_restore(base);
      putchar('\n');
    }
    update_jobs(job_list);
    shps1 = expand_ps1(getvar("PS1"));
    mark = stack_mark();
    line = lineread(shps1 ? shps1 : " $ ");
    if (!line) {
      stack_restore(mark);
      break;
    }
    n = strlen(line);
    acc = st_alloc(n + 2);
    memcpy(acc, line, n);
    acc[n] = '\n';
    acc[n + 1] = '\0';
    acclen = n + 1;
    free(line);

    for (;;) {
      accend = stack_mark(); /* bookmark after acc content */
      notclosed = 0;
      setinputstrn(acc, (int)acclen);
      {
        tmp = SHTOK(TNONE);
        m = stack_mark();
        do {
          lastt = tmp;
          if (lastt.type == TAND || lastt.type == TOR || lastt.type == TPIPE) {
          }
          chkwd |= CHKNL;
          tmp = tokenize();
        } while (tmp.type != TEOF);
        if (!notclosed &&
            (lastt.type == TAND || lastt.type == TOR || lastt.type == TPIPE))
          notclosed = 1;
        stack_restore(m);
      }
      popinput();
      if (!notclosed)
        break;

      stack_restore(accend); /* stnext is now right at end of acc content */
      line = lineread("> ");
      if (!line) {
        stack_restore(mark);
        //check if this is the right place to return error
        return 1;
      }
      n = strlen(line); /* append: new larger buffer, copy old + new */
      new = st_alloc(acclen + n + 2);
      memcpy(new, acc, acclen);
      memcpy(new + acclen, line, n);
      new[acclen + n] = '\n';
      new[acclen + n + 1] = '\0';
      free(line);
      acc = new;
      acclen += n + 1;
      if (!acc)
        break;
    }
    setinputstrn(acc, (int)acclen);
    inp = 1;
    simpsh_run();
    inp = 0;
    #ifdef READLINE
    append_history(1, histfile);
    #endif /* ifdef READLINE */
    popinput();
    stack_restore(mark);
  }
  //check if this is the right place to return lstatus
  return lstatus;
}

#ifdef READLINE
static void
readline_cleanup(void)
{
  clear_history();
  rl_free_line_state();
}

/** initialize history */
void
init_history(void)
{
  char buf[256];
  snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);
  using_history();
  if (access(histfile, W_OK) < 0) {
    static const char *histdir = ".local/state/simpsh";
    snprintf(buf, 256, "%s/%s", home, histdir);
    if (!pmkdir(buf))
      return;
    if (access(histfile, W_OK) < 0)
      if (write_history(histfile) != 0)
        return;
  } else {
    history_truncate_file(histfile, 1000);
    if (read_history_range(histfile, 0, -1) != 0)
      perror("Failed to read .simpsh_history");
  }
  atexit(readline_cleanup);
}

/** read line from interactive shell */
char *
lineread(char *prompt)
{
  char *line = readline(prompt);
  if (line && *line)
    add_history(line);

  return line;
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

