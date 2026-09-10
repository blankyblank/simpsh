/* fzrun.c — AFL forkserver + per-case scratch runner for simpsh exec fuzzing.
 *
 * Usage (via afl-fuzz, never directly for fuzzing):
 *   afl-fuzz ... -- /t/fzrun @@
 *
 * Protocol: on start, write a 4-byte plain handshake to fd 199, then loop:
 * read 4-byte command from fd 198 (EOF/error exits), mkdtemp scratch,
 * fork; grandchild-equivalent (child) chdirs, closes forkserver fds, execs
 * simpsh on the input; parent waits, cleans scratch, reports wait-status
 * on fd 199. Crash signals propagate via the reported status word, exactly
 * as if simpsh ran directly under afl-fuzz.
 */
#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define FORKSRV_FD 198  /* must match afl-fuzz (verified: /usr/include/afl/config.h) */

static int rm_one(const char *p, const struct stat *sb, int flag, struct FTW *f) {
  (void)sb; (void)flag; (void)f;
  return remove(p);
}

static int write_all(int fd, const void *buf, size_t n) {
  size_t off = 0;
  while (off < n) {
    ssize_t r = write(fd, (const char *)buf + off, n - off);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0)
      return -1;
    off += (size_t)r;
  }
  return 0;
}

static int read_all(int fd, void *buf, size_t n) {
  size_t off = 0;
  while (off < n) {
    ssize_t r = read(fd, (char *)buf + off, n - off);
    if (r < 0) {
      if (errno == EINTR)
        continue;
      return -1;
    }
    if (r == 0)
      return -1;
    off += (size_t)r;
  }
  return 0;
}

int main(int argc, char **argv) {
  const char *input;
  unsigned int hs = 0;  /* plain forkserver, no extended options */

  if (argc < 2)
    return 2;
  input = argv[1];

  if (write_all(FORKSRV_FD + 1, &hs, 4) < 0)
    return 2;

  for (;;) {
    unsigned int cmd = 0;
    char dir[] = "/tmp/fz.XXXXXX";
    pid_t pid;
    int st;

    (void)cmd;
    if (read_all(FORKSRV_FD, &cmd, 4) < 0)
      return 0;
    if (!mkdtemp(dir))
      return 2;
    pid = fork();
    if (pid < 0)
      return 2;
    if (pid == 0) {
      prctl(PR_SET_PDEATHSIG, SIGKILL);
      if (chdir(dir) < 0)
        _exit(2);
      close(FORKSRV_FD);
      close(FORKSRV_FD + 1);
      int ofd = open("/proc/self/oom_score_adj", O_WRONLY);
      if (ofd >= 0) {
        write(ofd, "1000", 4);
        close(ofd);
      }
      execl("/simpsh", "/simpsh", input, (char *)NULL);
      _exit(127);
    }
    if (write_all(FORKSRV_FD + 1, &pid, 4) < 0)
      return 2;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR)
      ;
    nftw(dir, rm_one, 16, FTW_DEPTH | FTW_PHYS);
    rmdir(dir);
    if (write_all(FORKSRV_FD + 1, &st, 4) < 0)
      return 2;
  }
}
