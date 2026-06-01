/* lex.h - tokenizer functions and declarations */
#ifndef LEX_H
#define LEX_H

#include <stdlib.h>
#include <stddef.h>

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
} token;

typedef enum {
  QNONE,
  QDOUBLE,
  QSINGLE,
  QCMDSUB,
  QCMDSUB_DQ,
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
extern int notclosed;
extern sh_tok last_tok;
extern int chkwd;
#define SHTOK(t) ((sh_tok){ .type = t, .sub = 0 })
#define SHREDIR(s) ((sh_tok){ .type = TREDIR, .sub = (s) })
#define SHWORD(w) ((sh_tok) { .type = TWORD, .cmd = w, .sub = 0 })

enum {
  CHKALIAS = 1 << 0,
  CHKNL = 1 << 1,
};

enum {
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

#endif /* LEX_H */
