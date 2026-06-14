/*  env.c - functions surrounding various parts of the shell environment  */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <limits.h>
#include <linux/limits.h>
#include <stddef.h>
#include <string.h>

#include "env.h"
#include "lex.h"
#include "main.h"
#include "parse.h"
#include "utils.h"

alias *alias_tab[ENV_BUCKETS];
shfunc *func_tab[ENV_BUCKETS];

wf *
wfdup(wf *s)
{
  wf *n;

  if (!s)
    return NULL;
  n = malloc(sizeof(wf));
  n->word = strndup_(s->word, s->len);
  n->len = s->len;
  n->qs = s->qs;
  n->next = wfdup(s->next);
  n->flags = s->flags;
  return n;
}

cmd_tree *
tree_dup(cmd_tree *s)
{
  cmd_tree *n;
  size_t cnt;

  if (!s)
    return NULL;

  n = malloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = s->type;
  n->flags = s->flags;

  switch (n->type) {
    case OP:
      COPP(n) = COPP(s);
      n->left = tree_dup(s->left);
      n->right = tree_dup(s->right);
      break;
    case SUBSHELL:
      n->left = tree_dup(s->left);
      break;
    case FUNC:
      CFUNC(n) = wfdup(CFUNC(s));
      n->left = tree_dup(s->left);
      break;
    case REDIR:
      CREDR(n) = redirdup(CREDR(s));
      n->left = tree_dup(s->left);
      break;
    case WHILE:
      n->left = tree_dup(s->left);
      n->right = tree_dup(s->right);
      break;
    case IF:
      n->left = tree_dup(s->left);
      n->right = tree_dup(s->right);
      CELSE(n) = tree_dup(CELSE(s));
      break;
    default:
      free(n);
      return NULL;
    case CMD:
      cnt = 0;
      for (size_t i = 0; CARGS(s)[i]; i++)
        cnt++;
      CARGS(n) = malloc((cnt + 1) * sizeof(wf *));
      for (size_t i = 0; i < cnt; i++)
        CARGS(n)[i] = wfdup(CARGS(s)[i]);
      CARGS(n)[cnt] = NULL;
      CVARC(n) = CVARC(s);
      if (CVARS(s)) {
        CVARS(n) = malloc((CVARC(s) + 1) * sizeof(wf *));
        for (size_t i = 0; i < CVARC(s); i++)
          CVARS(n)[i] = wfdup(CVARS(s)[i]);
        CVARS(n)[CVARC(s)] = NULL;
      } else {
        CVARS(n) = NULL;
      }
  }
  return n;
}

shfunc *
findfunc(const char *name)
{
  size_t i;
  shfunc *f;
  i = hash(name, ENV_BUCKETS);
  f = func_tab[i];

  while (f) {
    if (f->name[0] == name[0])
      if ((strcmp(f->name, name)) == 0)
        return f;
    f = f->next;
  }
  return NULL;
}

void
setfunc(const char *restrict name, cmd_tree *restrict body)
{
  shfunc *f, *n;
  size_t i;

  f = findfunc(name);
  if (f) {
    free_tree(f->body);
    free(f->name);
    f->name = strdup_(name);
    f->body = tree_dup(body);
  } else {
    i = hash(name, ENV_BUCKETS);
    n = malloc(sizeof(shfunc));
    if (!n)
      return;
    n->name = strdup_(name);
    n->body = tree_dup(body);
    n->next = func_tab[i];
    func_tab[i] = n;
  }

}

/** find alias by name */
alias *
findalias(const char *name)
{
  unsigned int i;
  alias *a;
  i = hash(name, ENV_BUCKETS);
  a = alias_tab[i];

  while (a) {
    if (a->name[0] == name[0])
      if (strcmp(a->name, name) == 0)
        return a;
    a = a->next;
  }
  return NULL;
}

/** set alias value */
void
setalias(const char *restrict name, const char *restrict val)
{
  alias *a;

  if ((a = findalias(name))) {
    free(a->value);
    a->value = strdup_(val);
  } else {
    if (!(a = malloc(sizeof(alias))))
      return;
    a->name = strdup_(name);
    a->value = strdup_(val);
    unsigned int i = hash(name, ENV_BUCKETS);
    a->next = alias_tab[i];
    alias_tab[i] = a;
  }
}

/** remove function */
void
rmfunc(const char *name)
{
  size_t i;
  shfunc **prev;
  shfunc *f;

  i = hash(name, ENV_BUCKETS);
  prev = &func_tab[i];
  f = func_tab[i];
  while (f) {
    if (f->name[0] == name[0])
      if (strcmp(f->name, name) == 0) {
        *prev = f->next;
        free(f->name);
        free_tree(f->body);
        free(f);
        return;
      }
    prev = &f->next;
    f = f->next;
  }
}

/** remove alias */
void
rmalias(const char *name)
{
  size_t i;
  alias **prev;
  alias *a;

  i = hash(name, ENV_BUCKETS);
  prev = &alias_tab[i];
  a = alias_tab[i];
  while (a) {
    if (a->name[0] == name[0])
      if (strcmp(a->name, name) == 0) {
        *prev = a->next;
        free(a->name);
        free(a->value);
        free(a);
        return;
      }
    prev = &a->next;
    a = a->next;
  }
}

int
aliascmd(char **args)
{
  int i;
  alias *e;
  char *delem, *n, *v;

  if (!args[1]) {
    for (i = 0; i < ENV_BUCKETS; i++) {
      if (alias_tab[i]) {
        e = alias_tab[i];
        while (e) {
          printf("alias %s=%s\n", e->name, e->value);
          e = e->next;
        }
      }
    }
    return 0;
  }

  if (!(delem = strchr(args[1], '='))) {
    if (!(e = findalias(args[1])))
      return 1;
    else
      printf("alias %s=%s\n", e->name, e->value);
  } else {
    n = strndup_(args[1], strlen(args[1]) - strlen(delem));
    v = strdup_(delem + 1);
    setalias(n, v);
    free(n);
    free(v);
  }

  return 0;
}

int
unaliascmd(char **argv)
{
  alias *e;
  /* int i;
   char *n, *v; */

  e = findalias(argv[1]);
  if (e) {
    rmalias(argv[1]);
  } else {
    shwarnx(argv[0], "alias not found");
    return 1;
  }

  return 0;
}
