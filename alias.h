#ifndef ALIAS_H
#define ALIAS_H

#define ALIAS_BUCKETS 64
#define MAX_ALIAS_DEPTH 10

typedef struct alias alias;
struct alias {
  char *name;
  char *value;
  alias *next;
};

extern alias *alias_tab[ALIAS_BUCKETS];

extern alias *lookup_alias(const char *);
extern void set_alias(const char *, const char *);
extern void rm_alias(const char *);

// vim: set filetype=c:
#endif /* ALIAS_H */
