#ifndef TEST_H
#define TEST_H

/* clang-format off */
typedef enum {
    TEND = 0,
    /* unary operators */
    TSTNZ, TSTZ,        /* -n, -z */
    TFILAXST, TFILEXST,    /* -a (exists), -e (exists) */
    TFILREG, TFILID,       /* -f (regular), -d (directory) */
    TFILBDEV, TFILCDEV,    /* -b (block), -c (char) */
    TFILFIFO, TFILSOCK,    /* -p (fifo), -S (socket) */
    TFILSYM,                 /* -L/-h (symlink) */
    TFILRD, TFILWR, TFILEX, /* -r, -w, -x */
    TFILGZ, TFILTT,        /* -s (size>0), -t (tty) */
    TFILSETU, TFILSETG, TFILSTCK, /* -u, -g, -k (setuid/gid/sticky) */
    TFILUID, TFILGID,      /* -O, -G (owner/group) */
    /* binary operators */
    TSTEQ, TSTNEQ,        /* =, != */
    TSTLT, TSTGT,          /* <, > */
    TINTEQ, TINTNE,        /* -eq, -ne */
    TINTGT, TINTGE, TINTLT, TINTLE, /* -gt, -ge, -lt, -le */
    TFILEQ, TFILNT, TFILOT /* -ef, -nt, -ot */
} testop;
/* clang-format on */

typedef enum {
  TM_OR,     /* -o */
  TM_AND,    /* -a */
  TM_NOT,    /* ! */
  TM_OPAREN, /* ( */
  TM_CPAREN, /* ) */
  TM_UNOP,   /* unary operator */
  TM_BINOP,  /* binary operator */
  TM_END     /* end of input */
} meta;

#define terr (1 << 0)

typedef struct {
  int flags;
  char **pos;
  char **wpend;
} testvar;

testop istestop(const char *, int);
int testeval(testvar *, testop, const char *, const char *);
int parse_test(testvar *);
extern int testcmd(char **);

#endif /* TEST_H */

