/* expand.c - string expandsion logic */
#include "malloc.h"
#include <string.h>
#define _POSIX_C_SOURCE 200809L
#include "env.h"
#include "expand.h"

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
__attribute__((hot)) char *
expand_word(wf *wordf)
{
  size_t end, i;
  size_t len;
  wf *f;
  // char *s;
  char *expanded;

  len = 0;
  for (f = wordf; f; f = f->next) {
    switch (f->qs) {
      case QSINGLE:
        if (f->len >= stleft)
          grow_stack(f->len);
        memcpy(stnext, f->word, f->len);
        stnext += f->len;
        stleft -= f->len;
        len += f->len;
        break;
      case QDOUBLE:
      case QNONE:
        i = 0;
        while (i < f->len) {
          size_t s, r;
          s = i;
          if (f->qs == QNONE) {
            while (i < f->len && f->word[i] != '$' && f->word[i] != '~')
              i++;
          } else {
            while (i < f->len && f->word[i] != '$')
              i++;
          }
          r = i - s;
          if (r) {
            if (r >= stleft)
              grow_stack(r);
            memcpy(stnext, f->word + s, r);
            stnext += r;
            stleft -= r;
            len += r;
          }
          if (i>= f->len)
            continue;
          if (f->word[i] == '~' && f->qs == QNONE) {
            expanded = exp_tilde(f->word, i, &end);
            if (expanded) {
              size_t elen;
              elen = strlen(expanded);
              if (elen >= stleft)
                grow_stack(elen);
              memcpy(stnext, expanded, elen);
              stnext += elen;
              stleft -= elen;
              len += elen;
              i = end;
            } else {
              st_putc('~');
              len++;
              i++;
            }
          } else if (f->word[i] != '$') {
            st_putc(f->word[i]);
            len++;
            i++;
          } else {
            expanded = exp_var(f->word, i, &end);
            if (expanded) {
              size_t vlen;
              vlen = strlen(expanded);
              if (vlen >= stleft)
                grow_stack(vlen);
              memcpy(stnext, expanded, vlen);
              stnext += vlen;
              stleft -= vlen;
              len += vlen;
              i = end;
            } else {
              i = end;
            }
          }
        }
        break;
    }
  }

  return grab_str(len);
}
