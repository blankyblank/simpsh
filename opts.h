#ifndef OPTS_H
#define OPTS_H

typedef union {
  struct {
    unsigned a_flag     : 1;
    unsigned b_flag     : 1;
    unsigned C_flag     : 1;
    unsigned e_flag     : 1;
    unsigned f_flag     : 1;
    unsigned h_flag     : 1;
    unsigned i_flag     : 1;
    unsigned I_flag     : 1;
    unsigned m_flag     : 1;
    unsigned n_flag     : 1;
    unsigned s_flag     : 1;
    unsigned u_flag     : 1;
    unsigned v_flag     : 1;
    unsigned V_flag     : 1;
    unsigned x_flag     : 1;
    unsigned emacs_flag : 1;
    unsigned nolog_flag : 1;
    unsigned pipe_flag  : 1;
    unsigned debug_flag : 1;
  };
  unsigned bits;
} shopt;

#define GETSHOPT(n) ((SHOPTS >> (n)) & 1u)
#define SETSHOPT(n) (SHOPTS |= (1u << (n)))
#define CLRSHOPT(n) (SHOPTS &= ~(1u << (n)))
#define aflag (gstate.shopts.a_flag)
#define bflag (gstate.shopts.b_flag)
#define Cflag (gstate.shopts.C_flag)
#define eflag (gstate.shopts.e_flag)
#define fflag (gstate.shopts.f_flag)
#define hflag (gstate.shopts.h_flag)
#define iflag (gstate.shopts.i_flag)
#define Iflag (gstate.shopts.I_flag)
#define mflag (gstate.shopts.m_flag)
#define nflag (gstate.shopts.n_flag)
#define sflag (gstate.shopts.s_flag)
#define uflag (gstate.shopts.u_flag)
#define vflag (gstate.shopts.v_flag)
#define Vflag (gstate.shopts.V_flag)
#define xflag (gstate.shopts.x_flag)
#define pipeflag (gstate.shopts.pipe_flag)

#define OPTC 19
#define SHOPTC 16 /* short option count */

extern const char shoptch[OPTC];

extern void init_opts(void);
extern void freeshargv(void);
extern int chkopt(char *);

#endif /* OPTS_H */

