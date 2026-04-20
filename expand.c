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

char *
expand_word(wf *wordf)
{
  size_t end, idx;
  size_t len, wlen;
  wf *f;
  char *s;
  char *expanded;

  len = 0;
  for (f = wordf; f; f = f->next) {
    switch (f->qs) {
    case QSINGLE:
      for (s = f->word; *s; s++) {
        st_putc(*s);
        len++;
      }
      break;
    case QDOUBLE:
    case QNONE:
      idx = 0;
      wlen = strlen(f->word);

      while (idx < wlen) {
        if (f->word[idx] != '$') {
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
            st_putc('$');
            len++;
            idx++;
          }
        }
      }
      break;
    }
  }

  return grab_str(len);
}
