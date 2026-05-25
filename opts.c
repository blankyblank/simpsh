#define _POSIX_C_SOURCE 200809L
#include <unistd.h>

#include "opts.h"

char shopts[OPTC];
const char *shoptname[OPTC] = {
  "allexport",
  "notify",
  "noclobber",
  "errexit",
  "noglob",
  "hashall",
  "interactive",
  "ignoreeof",
  "monitor",
  "noexec",
  "stdin",
  "nounset",
  "verbose",
  "vi",
  "xtrace",
  "emacs",
  "nolog",
  "pipefail",
  "debug",
};

const char shoptch[OPTC] = {
  'a',
  'b',
  'C',
  'e',
  'f',
  'h',
  'i',
  'I',
  'm',
  'n',
  's',
  'u',
  'v',
  'V',
  'x',
};

void
init_opts(void)
{
  for (int i = 0; i < OPTC; i++)
    shopts[i] = 0;

  iflag = isatty(STDIN_FILENO);
  mflag = iflag;
}
