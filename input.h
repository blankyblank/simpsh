#ifndef INPUT_H
#define INPUT_H

#include <stddef.h>

/** holds unified input stream data */
typedef struct shinput shinput;
struct shinput {
  shinput *prev;   /* Links to previous input */
  char *buf;       /* The raw read buffer */
  char *nchar;     /* Pointer to next unconsumed char in buf */
  size_t nleft;    /* Remaining chars in buf */
  char *strn;      /* String source (for -c, or pushing a line from readline) */
  size_t strnpos;  /* Position in string source */
  size_t strnleft; /* Remaining chars in string source */
  int fd;          /* File descriptor for script file input */
  int eof;         /* EOF flag */
  int linenum;     /* Line counter for error reporting */
  char ungetbuf[4]; /* Small pushback buffer */
  int unget;        /* number to pushback */
};

extern shinput *cur_shinpt;
#define SHEOF -1
#define BASEBUFSIZE BUFSIZ
#define shinput_linenum() (cur_shinpt ? cur_shinpt->linenum : 0)
#define popinput() (cur_shinpt = cur_shinpt->prev)

extern int shgetchar(void);
extern int shungetc(int);
extern void setinputstrn(char *, int);
extern void setinputf(int);
extern void init_input(void);

#endif  // INPUT_H
