#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include <stddef.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "main.h"
#include "test.h"
#include "utils.h"

struct t_op {
	char	optxt[4];
	testop	opnum;
};

static const struct t_op uops [] = {
	{"-a",	TFILAXST },
	{"-b",	TFILBDEV },
	{"-c",	TFILCDEV },
	{"-d",	TFILID },
	{"-e",	TFILEXST },
	{"-f",	TFILREG },
	{"-G",	TFILGID },
	{"-g",	TFILSETG },
	{"-h",	TFILSYM },
	// {"-H",	TFILCDF },
	{"-k",	TFILSTCK },
	{"-L",	TFILSYM },
	{"-n",	TSTNZ },
	{"-O",	TFILUID },
	// {"-o",	TOPTION },
	{"-p",	TFILFIFO, },
	{"-r",	TFILRD },
	{"-s",	TFILGZ },
	{"-S",	TFILSOCK },
	{"-t",	TFILTT },
	{"-u",	TFILSETU },
	{"-w",	TFILWR },
	{"-x",	TFILEX },
	{"-z",	TSTZ },
	{"",	TEND }
};

static const struct t_op bops [] = {
	{"=",	TSTEQ },
	{"==",	TSTEQ },
	{"!=",	TSTNEQ },
	{"<",	TSTLT },
	{">",	TSTGT },
	{"-eq",	TINTEQ },
	{"-ne",	TINTNE },
	{"-gt",	TINTGT },
	{"-ge",	TINTGE },
	{"-lt",	TINTLT },
	{"-le",	TINTLE },
	{"-ef",	TFILEQ },
	{"-nt",	TFILNT },
	{"-ot",	TFILOT },
	{"",	TEND }
};

static int oexpr(testvar *tv);
static int aexpr(testvar *tv);
static int nexpr(testvar *tv);
static int primary(testvar *tv);

testop
istestop(const char *s, int isunry)
{
  const struct t_op *op;
  op = isunry ? uops : bops;
    for (; op->opnum != TEND; op++)
      if (s[1] == op->optxt[1] && strcmp(s, op->optxt) == 0)
        return op->opnum;
  return TEND;
}

int
testeval(testvar *tv, testop op, const char *opnd1, const char *opnd2)
{
  long long int1, int2;
  struct stat st, st2;

  switch (op) {
    case TSTNZ:
      return *opnd1 != '\0';
    case TSTZ:
      return *opnd1 == '\0';
    case TSTEQ:
      return (strcmp(opnd1, opnd2) == 0);
    case TSTNEQ:
      return (strcmp(opnd1, opnd2) != 0);
    case TSTLT:
      return (strcmp(opnd1, opnd2) < 0);
    case TSTGT:
      return (strcmp(opnd1, opnd2) > 0);
    case TINTEQ:
      if (atoll_(opnd1, &int1) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (atoll_(opnd2, &int2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (int1 == int2);
    case TINTNE:
      if (atoll_(opnd1, &int1) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (atoll_(opnd2, &int2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (int1 != int2);
    case TINTGT:
      if (atoll_(opnd1, &int1) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (atoll_(opnd2, &int2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (int1 > int2);
    case TINTGE:
      if (atoll_(opnd1, &int1) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (atoll_(opnd2, &int2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (int1 >= int2);
    case TINTLT:
      if (atoll_(opnd1, &int1) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (atoll_(opnd2, &int2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (int1 < int2);
    case TINTLE:
      if (atoll_(opnd1, &int1) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (atoll_(opnd2, &int2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (int1 <= int2);
    case TFILAXST:
    case TFILEXST:
      return (!stat(opnd1, &st));
    case TFILREG:
      return (!stat(opnd1, &st) && S_ISREG(st.st_mode));
    case TFILID:
      return (!stat(opnd1, &st) && S_ISDIR(st.st_mode));
    case TFILBDEV:
      return (!stat(opnd1, &st) && S_ISBLK(st.st_mode));
    case TFILCDEV:
      return (!stat(opnd1, &st) && S_ISCHR(st.st_mode));
    case TFILFIFO:
      return (!stat(opnd1, &st) && S_ISFIFO(st.st_mode));
    case TFILSOCK:
      return (!stat(opnd1, &st) && S_ISSOCK(st.st_mode));
    case TFILSYM:
      return (!lstat(opnd1, &st) && S_ISLNK(st.st_mode));
    case TFILRD:
      return (!access(opnd1, R_OK));
    case TFILWR:
      return (!access(opnd1, W_OK));
    case TFILEX:
      if (access(opnd1, X_OK) != 0)
        return 0;
      if (!geteuid()) {
        if (stat(opnd1, &st) != 0)
          return 0;
        if (S_ISDIR(st.st_mode))
          return 1;
        return (st.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH)) != 0;
      }
      return 1;
    case TFILGZ:
      return (!stat(opnd1, &st) && st.st_size > 0);
    case TFILTT:
      return isatty(atoi_(opnd1));
    case TFILSETU:
      return (!stat(opnd1, &st) && (st.st_mode & S_ISUID));
    case TFILSETG:
      return (!stat(opnd1, &st) && (st.st_mode & S_ISGID));
    case TFILSTCK:
      return (!stat(opnd1, &st) && (st.st_mode & S_ISVTX));
    case TFILUID:
      return (!stat(opnd1, &st) && st.st_uid == geteuid());
    case TFILGID:
      return (!stat(opnd1, &st) && st.st_gid == getegid());
    case TFILEQ:
      if (stat(opnd1, &st) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (stat(opnd2, &st2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (st.st_dev == st2.st_dev && st.st_ino == st2.st_ino);
    case TFILNT:
      if (stat(opnd1, &st) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (stat(opnd2, &st2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (st.st_mtim.tv_sec > st2.st_mtim.tv_sec ||
              (st.st_mtim.tv_sec == st2.st_mtim.tv_sec &&
               st.st_mtim.tv_nsec > st2.st_mtim.tv_nsec));
    case TFILOT:
      if (stat(opnd1, &st) < 0) {
        tv->flags |= terr;
        return 0;
      }
      if (stat(opnd2, &st2) < 0) {
        tv->flags |= terr;
        return 0;
      }
      return (st.st_mtim.tv_sec < st2.st_mtim.tv_sec ||
              (st.st_mtim.tv_sec == st2.st_mtim.tv_sec &&
               st.st_mtim.tv_nsec < st2.st_mtim.tv_nsec));
      default:
        tv->flags |= terr;
        return 0;
  }
}

static int
nexpr(testvar *tv)
{
  if (tv->pos < tv->wpend && **tv->pos == '!') {
    tv->pos++;
    return !nexpr(tv);
  }

  return primary(tv);

}

static int
aexpr(testvar *tv) {
  int res;

  res = nexpr(tv);

  if (tv->pos < tv->wpend  && (*tv->pos)[0] == '-' && (*tv->pos)[1] == 'a' && (*tv->pos)[2] == '\0') {
    tv->pos++;
    if (!res)
      return 0;
    return aexpr(tv);
  }
  return res;
}

static int
oexpr(testvar *tv)
{
  int res;

  res = aexpr(tv);
  if (tv->pos < tv->wpend && **tv->pos == '-' && (*tv->pos)[1] == 'o' && (*tv->pos)[2] == '\0') {
    tv->pos++;
    if (res)
      return 1;
    return oexpr(tv);
  }
  return res;
}

static int
primary(testvar *tv)
{
  int res;
  testop op;
  const char *opnd1, *opnd2;

  if (tv->pos >= tv->wpend) {
    tv->flags |= terr;
    return 0;
  }

  res = 0;
  if (**tv->pos == '(') {
    tv->pos++;
    res = oexpr(tv);
    if (tv->pos >= tv->wpend || **tv->pos != ')') {
      tv->flags |= terr;
      return 0;
    }
    tv->pos++;
    return res;
  }

  if ((op = istestop(*tv->pos, 1)) != TEND) {
    const char *opnd;
    tv->pos++;
    if (tv->pos >= tv->wpend) {
      tv->flags |= terr;
      return 0;
    }
    opnd = *tv->pos++;
    return testeval(tv, op, opnd, NULL);
  }

  opnd1 = *tv->pos++;
  if ((op = istestop(*tv->pos, 0)) != TEND) {
    tv->pos++;
    if (tv->pos >= tv->wpend) {
      tv->flags |= terr;
      return 0;
    }
    opnd2 = *tv->pos++;
    return testeval(tv, op, opnd1, opnd2);
  }

  return testeval(tv, TSTNZ, opnd1, NULL);
}

int
parse_test(testvar *tv) {
  int res;

  res = oexpr(tv);
  if (tv->pos < tv->wpend) {
    tv->flags |= terr;
    return 0;
  }
  if (tv->flags & terr)
    return 0;
  return res;
}

int
testcmd(char **argv) {
  size_t argc = 0;
  int res;
  testvar tv, fasttv;

  argc = array_len(argv);
  if (*argv[0] == '[' && argv[0][1] == '\0') {
    if (argv[argc - 1][0] != ']' || argv[argc - 1][1] != '\0') {
      shwarnx("[", "missing ']'");
      return 2;
    }
    argc--;
  }
  if (argc <= 1)
    return 1;

  fasttv.flags = 0;
  if (argc == 2) {
    testop op = istestop(argv[1], 1);
    if (op != TEND) {
      res = testeval(&fasttv, op, argv[1], NULL);
      if (!(fasttv.flags & terr))
        return res ? 0 : 1;
    }
  } else if (argc == 3) {
    testop op = istestop(argv[2], 0);
    if (op != TEND) {
      return testeval(&fasttv, op, argv[1], argv[2]);
      if (!(fasttv.flags & terr))
        return res ? 0 : 1;
    }
  }


  tv.flags = 0;
  tv.pos = argv + 1;
  tv.wpend = argv + argc;
  res = parse_test(&tv);
  if (tv.flags & terr) {
    shwarnx(*argv, "syntax error");
    return 2;
  }
  return res ? 0 : 1;
}
