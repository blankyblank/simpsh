/* simpsh.c - functions for running the shell */
#include "exec.h"
#include "input.h"
#include "lex.h"
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
#include "env.h"

void
getbuildinfo(void) {
  printf("%s build info:\n"
         "build date: %s %s\n"
         "ansi C standard conformance: %ld\n",
         sh_argv0, __DATE__, __TIME__, __STDC_ISO_10646__);
}

/** make directory path */
static int pmkdir(char *path);

#ifdef READLINE
/** initialize history */
void
init_history(void)
{
  char histfile[265];
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


/** take in ps1 char * to do variable expansion for prompt */
char *
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

// int
// sh_ccmd(char *argv)
// {
//   setinputstrn(argv, strlen(argv));
//   simpsh_run(argv);
//   
// }

void
sh_interactive(void)
{
#ifdef READLINE
  init_history(); /* set up history */
#endif            /* ifdef READLINE */

  char *line, *acc, *new;
  stmark mark, accend, m;
  size_t n, acclen;
  sh_tok *toks;
  token last;

  for (;;) {
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
      { /* lightweight: tokenize to check completeness */
        int tc;
        m = stack_mark();
        toks = tokenize(&tc);
        if (!notclosed && toks && tc > 0) {
          last = toks[tc - 1].type;
          if (last == TAND || last == TOR || last == TPIPE)
            notclosed = 1;
        }
        stack_restore(m);
      }
      popinput();
      if (!notclosed)
        break;

      stack_restore(accend); /* stnext is now right at end of acc content */
      line = lineread("> ");
      if (!line) {
        stack_restore(mark);
        return;
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
    simpsh_run(acc); /* second tokenization, runs the command */
    stack_restore(mark);
  }
  return;
}

void
simpsh_core(void)
{
  stmark mark;
  sh_tok *toks;
  cmd_tree *c;
  int tok_c;

  mark = stack_mark();
  toks = tokenize(&tok_c);
  if (toks && !notclosed) {
    size_t i;
    i = 0;
    c = parse_list(toks, tok_c, TEOF, &i);
    run_commands(c);
  }
  stack_restore(mark);
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
