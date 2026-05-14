/* simpsh - a simple posix shell */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <fcntl.h>
#include <locale.h>
#include <stdio.h>
#include <unistd.h>
#ifdef READLINE
  #include <readline/history.h>
#endif /* ifdef READLINE */

#include "arg.h"
#include "env.h"
#include "exec.h"
#include "input.h"
#include "lex.h"
#include "main.h"
#include "malloc.h"
#include "simpsh.h"
#include "utils.h"

/* shell variables */
int sh_argc;
pid_t sh_pid;
char *sh_pid_s = NULL;
char *sh_argv0;
char *shps1;
char **sh_argv;
char *home;
int lstatus;

/* runtime state */
char histfile[265];
static int interactive = 1;
int builtin_tab[BUILTIN_BUCKETS];

__attribute__((always_inline)) static inline void simpsh_core(void);
static char *read_accumulate(void);
#define simpsh_run(l) setinputstrn(l, strlen(l)); simpsh_core(); popinput();

/** shell entry point */
int
main(int argc, char **argv)
{
  (void)argc;
  char *acc;
  int flags, fd;
  stmark mark;

  flags = 0;
  ARGBEGIN
  {
    case 'c':
      flags |= FLAG_c;
      break;
    case 'V':
      getbuildinfo();
      exit(0);
    default:
      fprintf(stderr, "%s: %c: bad option\n", argv0, ARGC());
      exit(1);
  }
  ARGEND

  if (!isatty(STDIN_FILENO))
    flags |= FLAG_t;
  if (flags & (FLAG_c | FLAG_t))
    interactive = 0;

  setlocale(LC_ALL, "");
  /* set up allocator */
  init_stack();
  /* set up shell variables */
  init_env();
  /* set up builtin table */
  init_builtins();
  /* set up input layer */
  init_input();

  snprintf(histfile, 265, "%s/.local/state/simpsh/simpsh_history", home);

  if (flags & FLAG_c) {
    setinputstrn(argv[0], strlen(argv[0]));
    simpsh_run(argv[0]);
    exit(lstatus);
  } else if (*argv) {
    fd = open(*argv, O_RDONLY);
    if (fd < 0) {
      perror("simpsh");
      exit(1);
    }
    setinputf(fd);
    simpsh_core();
    close(fd);
    exit(lstatus);
  } else if (flags & FLAG_t) {
    setinputf(STDIN_FILENO);
    simpsh_core();
    exit(lstatus);
  } else {
    /* run the main loop */
#ifdef READLINE
    init_history(); /* set up history */
#endif              /* ifdef READLINE */
    for (;;) {
      shps1 = expand_ps1(getvar("PS1"));
      mark = stack_mark();
      acc = read_accumulate();
      if (!acc)
        break;
      simpsh_run(acc); /* second tokenization, runs the command */
      stack_restore(mark);
    }
  }
}

static char *
read_accumulate(void)
{
  char *line, *acc, *new;
  stmark mark, accend, m;
  size_t n, acclen;
  sh_tok *toks;
  token last;

  mark = stack_mark();
  line = lineread(shps1 ? shps1 : " $ ");
  if (!line) {
    stack_restore(mark);
    return NULL;
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
      m = stack_mark();
      int tc;
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
      return acc;
    /* Undo setinputstrn's arena allocation */
    stack_restore(accend);
    /* stnext is now right at end of acc content */
    line = lineread("> ");
    if (!line) {
      stack_restore(mark);
      return NULL;
    }
    /* append: new larger buffer, copy old + new */
    n = strlen(line);
    new = st_alloc(acclen + n + 2);
    memcpy(new, acc, acclen);
    memcpy(new + acclen, line, n);
    new[acclen + n] = '\n';
    new[acclen + n + 1] = '\0';
    free(line);
    acc = new;
    acclen += n + 1;
  }
}

static inline void
simpsh_core(void)
{
  stmark mark;
  sh_tok *toks;
  cmd_tree *c;
  int tok_c;

  mark = stack_mark();
  toks = tokenize(&tok_c);
  if (toks && !notclosed) {
    c = build_tree(toks, tok_c, TEOF);
    run_commands(c);
  }
  stack_restore(mark);
}
