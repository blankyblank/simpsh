#include "config.h"
#if ENABLE_SLEEP
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <ctype.h>
#include <time.h>

#include "errmsg.h"

#define MIN 60
#define HOUR 3600
#define DAY 86400

int
sleepcmd(char *argv[])
{
  char *argv0 = *argv++;
  struct timespec ts = (struct timespec) { .tv_sec = 0, .tv_nsec = 0 };
  struct timespec rem = (struct timespec) { .tv_sec = 0, .tv_nsec = 0 };

  if (!*argv)
    return shwarn(argv0, "requires an argument");
  for (; *argv; argv++) {
    char *arg = *argv;
    int sec, nsec;
    int mult = 1, dot = 0, denom = 10;
    int dec = 0, whole = 0;

    for (char *p = arg; *p; p++) {
      if (isdigit(*p)) {
        if (dot) {
          denom *= 10;
          dec = dec * 10 + (*p - '0');
        } else {
          whole = whole * 10 + (*p - '0');
        }
        continue;
      }
      
      switch (*p) {
        case '.':
          dot = 1;
          continue;
        case 'M':
        case 'm':
          mult = MIN;
          *p = '\0';
          break;
        case 'H':
        case 'h':
          mult = HOUR;
          *p = '\0';
          break;
        case 'D':
        case 'd':
          mult = DAY;
          *p = '\0';
          break;
        default:
          return shwarn_arg(argv0, arg, "invalid time interval");
      }
    }
    sec = whole * mult + (dec * mult) / denom;
    nsec = ((dec * mult) % denom) * 1000000000 / denom;
    ts.tv_sec += sec;
    ts.tv_nsec += nsec;
  }
  ts.tv_sec += ts.tv_nsec / 1000000000;
  ts.tv_nsec %= 1000000000;
  if (nanosleep(&ts, &rem) < 0) {
    if (errno != EINTR)
      return 1;
    while (nanosleep(&rem, &rem) < 0) {
      if (errno != EINTR)
        return 1;
    }
  }
  return 0;
}
#endif /* ENABLE_SLEEP */
