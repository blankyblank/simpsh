/* expand.c - string expandsion logic */
#include "simpsh.h"
#include "lex.h"
#include "env.h"
#include "expand.h"
#include "utils.h"

void
bufcat(char **buf, size_t *bufsize, size_t *buflen, const char *src, size_t n)
{
  if (*buflen + n + 1 > *bufsize) {
    *buf = s_realloc(*buf, bufsize);
    if (!*buf)
      return;
  }
  memcpy(*buf + *buflen, src, n);
  *buflen += n;
  (*buf)[*buflen] = '\0';
}

char **
expand_argv(wf **args)
{
  fprintf(stderr, "expand_argv: entering, args=%p\n", (void*)args);
  size_t i, argc = 0;
  while (args[argc])
    argc++;

  char **argv = st_alloc((argc + 1) * sizeof(char *));
  for (i = 0; i < argc; i++) {
    fprintf(stderr, "expand_argv: calling expand_word for args[%zu]=%p\n", i, (void*)args[i]);
    argv[i] = expand_word(args[i]);
    if (!argv[i])
      return NULL;
  }
  argv[argc] = NULL;
  return argv;
}

char *
expand_word(wf *wordf)
{
  fprintf(stderr, "expand_word: wordf=%p\n", (void*)wordf);
  char *p = stack_ptr();
  char *expanded;
  size_t end, idx;
  size_t len;
  wf *f;
  char *s;

  for (f = wordf; f; f = f->next) {
    fprintf(stderr, "expand_word: f=%p, f->qs=%d, f->word=%p\n", (void*)f, f->qs, (void*)f->word);
    switch (f->qs) {
    case QSINGLE:
      for (s = f->word; *s; s++)
        p = st_putc(*s, p);
      break;
    case QDOUBLE:
    case QNONE:
      idx = 0;
      len = strlen(f->word);
      while (idx < len) {
        if (f->word[idx] != '$') {
          p = st_putc(f->word[idx], p);
          idx++;
        } else {
          expanded = exp_var(f->word, idx, &end);
          if (expanded) {
            for (s = expanded; *s; s++)
              p = st_putc(*s, p);
            idx = end;
          } else {
            p = st_putc('$', p);
            idx++;
          }
        }
      }
      break;
    }
  }

  return grab_str(p);
}
