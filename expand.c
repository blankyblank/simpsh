#include "simpsh.h"
#include "lex.h"
#include "env.h"
#include "expand.h"
#include "utils.h"


void
bufcat(char **buf, size_t *bufsize, size_t *buflen, const char *src, size_t n) {
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
expand_argv(wf **args) {
  size_t i, argc = 0;
  while (args[argc])
    argc++;

  char **argv = malloc((argc + 1) * sizeof(char *));
  for (i = 0; i < argc; i++) {
    argv[i] = expand_word(args[i]);
    if (!argv[i]) {
      freeptr(argv);
      return NULL;
    }
  }
  argv[argc] = NULL;
  return argv;
}

char *
expand_word(wf *wordf) {
  size_t bufsize = BUF_S;
  size_t buflen = 0, len;
  size_t end, p;
  size_t cp_len, next_v;
  char *buf = malloc(bufsize), *expanded;
  if (!buf)
    return NULL;
  buf[0] = '\0';

  for (wf *f = wordf; f; f = f->next) {
    switch (f->qs) {
    case QSINGLE:
      len = strlen(f->word);
      bufcat(&buf, &bufsize, &buflen, f->word, len);
      if (!buf)
        return NULL;
      break;
    case QDOUBLE:
    case QNONE:
      p = 0;
      len = strlen(f->word);
      while (p < len) {
        if (f->word[p] != '$') {
          next_v = p;
          while (next_v < len && f->word[next_v] != '$')
            next_v++;
          cp_len = next_v - p;
          bufcat(&buf, &bufsize, &buflen, f->word + p, cp_len);
          if (!buf)
            return NULL;
          p = next_v;
        } else {
          expanded = exp_var(f->word, p, &end);
          if (expanded) {
            bufcat(&buf, &bufsize, &buflen, expanded, strlen(expanded));
            if (!buf)
              return NULL;
            free(expanded);
            p = end;
          } else {
            free(buf);
            return NULL;
          }
        }
      }
      break;
    }
  }

  return buf;
}

