#include "simpsh.h"
#include "alias.h"
#include "utils.h"

alias *alias_tab[ALIAS_BUCKETS];

alias *
lookup_alias(const char *name) {
  int i;
  alias *e;
  i = hash(name, ALIAS_BUCKETS);
  e = alias_tab[i];

  while (e != NULL) {
    if (strcmp(e->name, name) == 0)
      return e;
    e = e->next;
  }
  return e;
}

void
set_alias(const char *name, const char *val) {
  int i;
  alias *a;
  i = hash(name, ALIAS_BUCKETS);
  a = alias_tab[i];

  while (a != NULL) {
    if (strcmp(a->name, name) == 0) {
      free(a->value);
      a->value = strdup(val);
      return;
    }
    a = a->next;
  }

  a = malloc(sizeof(alias));
  a->name = strdup(name);
  a->value = strdup(val);
  a->next = alias_tab[i];
  alias_tab[i] = a;
}

void
rm_alias(const char *name) {
  alias *e, *prev = NULL;
  int i;

  i = hash(name, ALIAS_BUCKETS);
  e = alias_tab[i];

  while (e != NULL) {
    if (strcmp(e->name, name) == 0) {
      if (prev)
        prev->next = e->next;
      else
        alias_tab[i] = e->next;

      free(e->name);
      free(e->value);
      free(e);
      return;
    }
    prev = e;
    e = e->next;
  }
}
