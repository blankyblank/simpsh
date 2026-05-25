#ifndef OPTS_H
#define OPTS_H

#define OPTC 19

#define aflag shopts[0]
#define bflag shopts[1]
#define Cflag shopts[2]
#define eflag shopts[3]
#define fflag shopts[4]
#define hflag shopts[5]
#define iflag shopts[6]
#define Iflag shopts[7]
#define mflag shopts[8]
#define nflag shopts[9]
#define sflag shopts[10]
#define uflag shopts[11]
#define vflag shopts[12]
#define Vflag shopts[13]
#define xflag shopts[14]

extern char shopts[OPTC];
extern const char shoptch[OPTC];
extern const char *shoptname[OPTC];
extern void init_opts(void);

#endif /* OPTS_H */

