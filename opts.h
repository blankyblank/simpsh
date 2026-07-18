#ifndef OPTS_H
#define OPTS_H

#include "main.h"

#define aflag SHOPTS[0]
#define bflag SHOPTS[1]
#define Cflag SHOPTS[2]
#define eflag SHOPTS[3]
#define fflag SHOPTS[4]
#define hflag SHOPTS[5]
#define iflag SHOPTS[6]
#define Iflag SHOPTS[7]
#define mflag SHOPTS[8]
#define nflag SHOPTS[9]
#define sflag SHOPTS[10]
#define uflag SHOPTS[11]
#define vflag SHOPTS[12]
#define Vflag SHOPTS[13]
#define xflag SHOPTS[14]
#define pipeflag SHOPTS[17]

extern const char shoptch[OPTC];

extern void init_opts(void);
extern void freeshargv(void);
extern int chkopt(char *);

#ifndef MUSL
extern void getbuildinfo(void);
#endif /* ifndef MUSL */

#endif /* OPTS_H */

