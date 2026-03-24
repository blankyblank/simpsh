#include "simpsh.h"

alias *alias_tab[ALIAS_BUCKETS];

alias *
lookup_alias(char *name) {
  int i;
  alias *e;
  i = hash(name);
  e = alias_tab[i];

  while (e != NULL) {
    if (strcmp(e->name, name) == 0)
      return e;
    e = e->next;
  }
  return e;
}

void
set_alias(char *name, char *val) {
  int i;
  alias *e;
  i = hash(name);
  e = alias_tab[i];

  while (e != NULL) {
    if (strcmp(e->name, name) == 0) {
      free(e->value);
      e->value = strdup(val);
      return;
    }
    e = e->next;
  }

  e = malloc(sizeof(alias));
  e->name = strdup(name);
  e->value = strdup(val);
  e->next = alias_tab[i];
  alias_tab[i] = e;
}

void
rm_alias(char *name) {
  alias *e, *prev = NULL;
  int i;

  i = hash(name);
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
