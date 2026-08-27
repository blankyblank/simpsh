/*  env.c - functions surrounding various parts of the shell environment  */
#define _POSIX_C_SOURCE 200809L
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "alloc.h"
#include "main.h"
#include "pipe.h"
#include "env.h"
#include "errmsg.h"
#include "lex.h"
#include "parse.h"
#include "utils.h"

alias *alias_tab[ENV_BUCKETS];
shfunc *func_tab[ENV_BUCKETS];
wf * wfdup(wf *s);
static clause * clausedup(clause *c);
static void free_wf(wf *);

wf *
wfdup(wf *s)
{
  wf *n;

  if (!s)
    return NULL;
  n = salloc(sizeof(wf));
  if (s->qs == QCMDSUB || s->qs == QCMDSUB_DQ)
    n->cmdsub = s->cmdsub ? tree_dup(s->cmdsub) : NULL;
  else
    n->word = strndup_(s->word, s->len);
  n->len = s->len;
  n->qs = s->qs;
  n->next = wfdup(s->next);
  n->flags = s->flags;
  return n;
}

static clause *
clausedup(clause *c)
{
  size_t cnt = 0;
  clause *n;

  if (!c)
    return NULL;
  n = salloc(sizeof(clause));
  for (size_t i = 0; c->ptrn[i]; i++)
    cnt++;
  n->ptrn = salloc((cnt + 1) * sizeof(wf *));
  for (size_t i = 0; c->ptrn[i]; i++)
    n->ptrn[i] = wfdup(c->ptrn[i]);
  n->ptrn[cnt] = NULL;
  n->body = tree_dup(c->body);
  n->next = clausedup(c->next);
  return n;
}

cmd_tree *
tree_dup(cmd_tree *s)
{
  cmd_tree *n;
  size_t cnt;

  if (!s)
    return NULL;

  n = salloc(sizeof(cmd_tree));
  if (!n)
    return NULL;
  n->type = s->type;
  n->flags = s->flags;
  n->left = n->right = NULL;

  switch (n->type) {
    case OP:
      COPP(n) = COPP(s);
      if (COPP(s) == TPIPE) {
        CPIPE(n) = salloc((CPIPEC(s) + 1) * sizeof(cmd_tree *));
        if (!CPIPE(n))
          return NULL;
        for (size_t i = 0; i < CPIPEC(s); i++)
          CPIPE(n)[i] = tree_dup(CPIPE(s)[i]);
        CPIPEC(n) = CPIPEC(s);
        n->left = n->right = NULL;
      } else {
        n->left = tree_dup(s->left);
        n->right = tree_dup(s->right);
      }
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
    case BRACE:
      n->left = tree_dup(s->left);
      break;
    case CASE:
      CCASE(n).word = wfdup(CCASE(s).word);
      CCASE(n).clauses = clausedup(CCASE(s).clauses);
      break;
    case FOR:
      {
        size_t wrdc = 0;
        n->right = tree_dup(s->right);
        CFOR(n).name = wfdup(CFOR(s).name);
        if (CFOR(s).words) {
          array_len(CFOR(s).words, wrdc);
          CFOR(n).words = salloc((wrdc + 1) * sizeof(wf *));
          for (size_t i = 0; CFOR(s).words[i]; i++)
            CFOR(n).words[i] = wfdup(CFOR(s).words[i]);
          CFOR(n).words[wrdc] = NULL;
        } else {
          CFOR(n).words = NULL;
        }
      }
      break;
    case IF:
      n->left = tree_dup(s->left);
      n->right = tree_dup(s->right);
      CELSE(n) = tree_dup(CELSE(s));
      break;
    default:
      sfree(n);
      return NULL;
    case CMD:
      cnt = 0;
      for (size_t i = 0; CARGS(s)[i]; i++)
        cnt++;
      CARGS(n) = salloc((cnt + 1) * sizeof(wf *));
      for (size_t i = 0; i < cnt; i++)
        CARGS(n)[i] = wfdup(CARGS(s)[i]);
      CARGS(n)[cnt] = NULL;
      CVARC(n) = CVARC(s);
      if (CVARS(s)) {
        CVARS(n) = salloc((CVARC(s) + 1) * sizeof(wf *));
        for (size_t i = 0; i < CVARC(s); i++)
          CVARS(n)[i] = wfdup(CVARS(s)[i]);
        CVARS(n)[CVARC(s)] = NULL;
      } else {
        CVARS(n) = NULL;
      }
  }
  return n;
}

static void
free_wf(wf *f)
{
  if (!f)
    return;
  if (f->qs == QCMDSUB || f->qs == QCMDSUB_DQ)
    free_tree(f->cmdsub);
  else
    sfree(f->word);
  free_wf(f->next);
  sfree(f);
}

static inline void
free_redir(redir *r)
{
  while (r) {
    redir *tmp;
    tmp = r;
    r = r->next;
    free_wf(tmp->name);
    if (tmp->heredoc)
      sfree(tmp->heredoc);
    sfree(tmp);
  }
  return;
}

void
free_tree(cmd_tree *n)
{
  if (!n)
    return;

  switch (n->type) {
    case OP:
      if (COPP(n) == TPIPE) {
        for (size_t i = 0; i < CPIPEC(n); i++)
          free_tree(CPIPE(n)[i]);
        sfree(CPIPE(n));
      } else {
        free_tree(n->left);
        free_tree(n->right);
      }
      sfree(n);
      break;
    case SUBSHELL:
      free_tree(n->left);
      sfree(n);
      break;
    case FUNC:
      free_wf(CARGS(n)[0]);
      sfree(CARGS(n));
      free_tree(n->left);
      sfree(n);
      break;
    case REDIR:
      free_redir(CREDR(n));
      free_tree(n->left);
      sfree(n);
      break;
    case WHILE:
      free_tree(n->left);
      free_tree(n->right);
      sfree(n);
      break;
    case FOR:
      free_tree(n->right);
      if (CFOR(n).words) {
        for (size_t i = 0; CFOR(n).words[i]; i++)
          free_wf(CFOR(n).words[i]);
        sfree(CFOR(n).words);
      }
      free_wf(CFOR(n).name);
      sfree(n);
      break;
    case CASE:
      free_wf(CCASE(n).word);
      for (clause *c = CCASE(n).clauses; c;) {
        clause *next = c->next;
        for (size_t i = 0; c->ptrn[i]; i++)
          free_wf(c->ptrn[i]);
        sfree(c->ptrn);
        free_tree(c->body);
        sfree(c);
        c = next;
      }
      sfree(n);
      break;
    case IF:
      free_tree(n->left);
      free_tree(n->right);
      free_tree(CELSE(n));
      sfree(n);
      break;
    case BRACE:
      free_tree(n->left);
      sfree(n);
      break;
    case CMD:
      for (size_t i = 0; CARGS(n)[i]; i++)
        free_wf(CARGS(n)[i]);
      sfree(CARGS(n));
      if (CVARS(n)) {
        for (size_t i = 0; CVARS(n)[i]; i++)
          free_wf(CVARS(n)[i]);
        sfree(CVARS(n));
      }
      sfree(n);
      break;
    default:
      return;
  }
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
    sfree(f->name);
    f->name = strdup_(name);
    f->body = tree_dup(body);
  } else {
    i = hash(name, ENV_BUCKETS);
    n = salloc(sizeof(shfunc));
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

  if (fakectx)
    svfkalias(fkstate, name);
  if ((a = findalias(name))) {
    sfree(a->value);
    a->value = strdup_(val);
  } else {
    if (!(a = salloc(sizeof(alias))))
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
        sfree(f->name);
        free_tree(f->body);
        sfree(f);
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

  if (fakectx)
    svfkalias(fkstate, name);
  i = hash(name, ENV_BUCKETS);
  prev = &alias_tab[i];
  a = alias_tab[i];
  while (a) {
    if (a->name[0] == name[0])
      if (strcmp(a->name, name) == 0) {
        *prev = a->next;
        sfree(a->name);
        sfree(a->value);
        sfree(a);
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
    sfree(n);
    sfree(v);
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
    shwarn(argv[0], "alias not found");
    return 1;
  }

  return 0;
}
