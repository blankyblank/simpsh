#ifndef MALLOC_H
#define MALLOC_H

/* malloc and free or other small inlined functions */
static inline char *
s_realloc(char *buf, size_t *bufsize) {
  char *t;

  *bufsize *= 2;
  t = realloc(buf, *bufsize);
  if (!t) {
    free(buf);
    return NULL;
  }
  return t;
}

static inline void
freeptr(char **args) {
  if (args != NULL) {
    int i = 0;
    while (args[i] != NULL) {
      free(args[i]);
      i++;
    }
    free(args);
  }
}

// vim: set filetype=c:
#endif // !MALLOC_H
