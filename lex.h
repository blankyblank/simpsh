/* lex.h - tokenizer functions and declarations */
#ifndef LEX_H
#define LEX_H

#include <stdlib.h>
#include <stddef.h>
#include "utils.h"

enum chars {
  C_WORD,
  C_SPACE,
  C_NL,
  C_COMMENT,
  C_SQUOTE,
  C_DQUOTE,
  C_BSLASH,
  C_DOLLAR,
  C_EXCL,
  C_AMP,
  C_PIPE,
  C_SEMI,
  C_LP,
  C_RP,
  C_LB,
  C_RB,
  C_LT,
  C_GT,
};

typedef enum {
  TNONE,
  TNOT,
  TPIPE,
  TWORD,
  TAND,
  TOR,
  TSEMI,
  TNL,
  TEOF,
  TLP,
  TRP,
  TLB,
  TRB,
  TBKGRND,
  TREDIR,
} token;

typedef enum {
  QNONE,
  QDOUBLE,
  QSINGLE,
} quoted;

/**
 * word fragment
 * @field wf pointer to next node
 * @field string word
 * @field enum quoted qs
 * @field word length
 */
typedef struct wf wf;
struct wf {
  wf *next;
  char *word;
  quoted qs;
  size_t len;
};

/** store tokens before building argv */
typedef struct {
  wf *cmd;
  token type;
  int sub;
} sh_tok;

extern int alias_depth;
extern int func_depth;
extern int notclosed;
extern sh_tok last_tok;
extern int chkwd;
#define SHTOK(t) ((sh_tok){ .type = t, .sub = 0 })
#define SHREDIR(s) ((sh_tok){ .type = TREDIR, .sub = (s) })
#define SHWORD(w) ((sh_tok) { .type = TWORD, .cmd = w, .sub = 0 })

#define CHKALIAS (1 << 0)
#define CHKNL (1 << 1)

#define RDIN 1
#define RDOUT 2
#define RDAPP 3
#define RDDUPO 4
#define RDDUPI 5
#define RDRW 6
#define RDCLOB 7
#define RDHERE 8
#define RDHERE_D 9

extern sh_tok tokenize(void);
extern void pushstring(char *, size_t, int);
extern void popstring(void);
extern char *join_wf(wf *wordf);

static inline wf *
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
  return n;
}

#endif /* LEX_H */
