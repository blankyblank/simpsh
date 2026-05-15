/* expand.c - string expandsion logic */
#define _POSIX_C_SOURCE 200809L
#include "env.h"
#include "expand.h"
#include "utils.h"

/** expand argument vector */
char **
expand_argv(wf **args)
{
  size_t i, argc = 0;
  while (args[argc])
    argc++;

  char **argv = st_alloc((argc + 1) * sizeof(char *));
  for (i = 0; i < argc; i++) {
    argv[i] = expand_word(args[i]);
    if (!argv[i])
      return NULL;
  }
  argv[argc] = NULL;
  return argv;
}

/** expand word with variable substitution */
char *
expand_word(wf *wordf)
{
  size_t end, idx;
  size_t len;
  wf *f;
  char *s;
  char *expanded;

  len = 0;
  for (f = wordf; f; f = f->next) {
    switch (f->qs) {
    case QSINGLE:
      for (size_t i = 0; i < f->len; i++) {
        st_putc(f->word[i]);
        len++;
      }
      break;
    case QDOUBLE:
    case QNONE:
      idx = 0;

      while (idx < f->len) {
        if (f->word[idx] == '~' && f->qs == QNONE) {
          expanded = exp_tilde(f->word, idx, &end);
          if (expanded) {
            for (s = expanded; *s; s++) {
              st_putc(*s);
              len++;
            }
            idx = end;
          } else {
              st_putc('~');
              len++;
              idx++;
          }
        } else if (f->word[idx] != '$') {
          st_putc(f->word[idx]);
          len++;
          idx++;
        } else {
          expanded = exp_var(f->word, idx, &end);
          if (expanded) {
            for (s = expanded; *s; s++) {
              st_putc(*s);
              len++;
            }
            idx = end;
          } else {
            idx = end;
          }
        }
      }
      break;
    }
  }

  return grab_str(len);
}
