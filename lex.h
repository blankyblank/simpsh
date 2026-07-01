/* lex.h - tokenizer functions and declarations */
#ifndef LEX_H
#define LEX_H

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include "alloc.h"
#include "main.h"

enum chars {
  C_WORD,
  C_SPACE,
  C_NL,
  C_COMMENT,
  C_SQUOTE,
  C_DQUOTE,
  C_BSLASH,
  C_DOLLAR,
  C_AMP,
  C_PIPE,
  C_SEMI,
  C_LP,
  C_RP,
  C_LB,
  C_RB,
  C_LT,
  C_GT,
  C_BTICK,
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
  TCMDSUB,
  TIF,
  TTHEN,
  TELIF,
  TELSE,
  TFI,
  TCASE,
  TESAC,
  TDSEMI,
  TWHILE,
  TUNTIL,
  TFOR,
  TIN,
  TDO,
  TDONE,
} token;

typedef enum {
  QNONE = 1 << 0,
  QDOUBLE = 1 << 1,
  QSINGLE = 1 << 2,
  QHEREDOC = 1 << 3,
  QVAR = 1 << 4,
  QVAR_DQ = 1 << 5,
  QBRACE = 1 << 6,
  QBRACE_DQ = 1 << 7,
  QCMDSUB = 1 << 8,
  QCMDSUB_DQ = 1 << 9,
  QARITH = 1 << 10,
} quoted;

typedef enum {
  WFSINGLE = 1 << 0,
  WFDOUBLE = 1 << 1,
  WFALLNUM = 1 << 2,
  WFCMDSUB = 1 << 3,
} wf_flags;

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
  size_t len;
  quoted qs;
  int flags;
};

/** store tokens before building argv */
typedef struct {
  wf *cmd;
  token type;
  int sub;
} sh_tok;

extern wf *wf_chunk;
extern unsigned int wf_chunk_left;
extern ucharf alias_depth;
extern ucharf notclosed;
extern sh_tok last_tok;
extern ucharf chkwd;

#define WF_CHUNK_SIZE 4
#define SHTOK(t) ((sh_tok){ .type = t, .sub = 0 })
#define SHREDIR(s) ((sh_tok){ .type = TREDIR, .sub = (s) })
#define SHWORD(w) ((sh_tok) { .type = TWORD, .cmd = w, .sub = 0 })

enum {
  CHKALIAS = 1 << 0,
  CHKNL = 1 << 1,
  CHKKWD = 1 << 2,
};

enum rdr {
  RDIN = 1 << 0,
  RDOUT = 1 << 1,
  RDAPP = 1 << 2,
  RDDUPO = 1 << 3,
  RDDUPI = 1 << 4,
  RDRW = 1 << 5,
  RDCLOB = 1 << 6,
  RDHERE = 1 << 7,
  RDHERE_D = 1 << 8,
};

extern sh_tok tokenize(void);
extern void pushstring(char *, size_t, int);
extern void popstring(void);
extern char *join_wf(wf *wordf);

static inline wf *
wfalloc(void)
{
  if (wf_chunk_left == 0) {
    if (!(wf_chunk = st_alloc(WF_CHUNK_SIZE * sizeof(wf))))
      return NULL;
    wf_chunk_left = WF_CHUNK_SIZE;
  }
  wf *f = wf_chunk++;
  wf_chunk_left--;
  return f;
}

/**  add word fragment onto the end of the linked list  */
static inline void
append_wf(wf **restrict head, wf **restrict tail, char *restrict w, size_t len, int quoted)
{
  wf *f;
  if (!(f = wfalloc())) {
    perror("st_alloc failed");
    return;
  }
  f->word = w;
  f->len = len;
  f->qs = quoted;
  f->next = NULL;
  if (!*head)
    *head = f;
  else
    (*tail)->next = f;
  *tail = f;
}

#endif /* LEX_H */
