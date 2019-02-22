#define _GNU_SOURCE
#include <sys/wait.h>
#include <sys/utsname.h>
#include <sys/param.h>
#include <sched.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pty.h>
#include <fcntl.h>
#include <termios.h>

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)
#define BUF_SIZE 100

int main(void) {
  int masterFd;
  int slaveFd;
  pid_t child_pid;
  int numread;
  char name[MAXPATHLEN];
  char buf[BUF_SIZE];
  // O_NOCTTY: don't assign controlling terminal
  // or you can use `posix_openpt(3)`.
  if((masterFd = open("/dev/ptmx", O_RDONLY | O_NOCTTY)) == -1) {
    errExit("open /dev/ptmx");
  }
  // to open slave device, we should exec the psuedoterminal related functions; `grantpt(3)` & `unlockpt(3)`.
  if (grantpt(masterFd) == -1 || unlockpt(masterFd) == -1) {
    close(masterFd);
    errExit("grantpt or unlockpt");
  }

  // /dev/pts is mounted in devpts filesystem.
  // cat /proc/self/mountinfo
  // 24 23 0:21 / /dev/pts rw,nosuid,noexec,relatime shared:3 - devpts devpts rw,gid=5,mode=620,ptmxmode=000
  printf("atty: %s, ttyname(master): %s, ptsname(slave): %s\n", isatty(masterFd) ? "true" : "false", ttyname(masterFd), ptsname(masterFd));

  if(slaveFd = open(ptsname(masterFd), O_RDWR) == -1) {
    errExit("slave open");
  }
  close(masterFd); close(slaveFd);

  // Or you can use openpty to get master & slave psuedoterminal descriptor. Note that this function is not POSIX based function.
  // LDFLAGS=-lutil is required to compile.
  openpty(&masterFd, &slaveFd, name, NULL, NULL);
  printf("allocated pty '%s' (%d/%d)\n", name, masterFd, slaveFd);
  child_pid = fork();
  // parent
  if(child_pid) {
    close(slaveFd);
    if((numread = read(masterFd, buf, BUF_SIZE)) == -1) {
      errExit("read");
    }
    if (numread) {
      write(STDOUT_FILENO, "ok", 3);
      //write(STDOUT_FILENO, buf, numread); /* echo child's output to stdout  */
    }
  } else {
    // child
    #include <utmp.h>
    if(login_tty(slaveFd) == -1) {
      errExit("login_tty");
    }
    while(1) {
      printf("Feeling OK :-)\n");
      sleep(1);

    }

  }
  //if((child_pid = fork()) == 0) {
  //  if(setsid() == -1) {
  //    errExit("setsid");
  //  }
  //  close(masterFd);
  //  if(dup2(slaveFd, STDIN_FILENO) != STDIN_FILENO) {
  //    errExit("dup2 stdin");
  //  }
  //  if(dup2(slaveFd, STDOUT_FILENO) != STDOUT_FILENO) {
  //    errExit("dup2 stdout");
  //  }
  //  if(dup2(slaveFd, STDERR_FILENO) != STDERR_FILENO) {
  //    errExit("dup2 stderr");
  //  }
  //  if(slaveFd > STDERR_FILENO) {
  //    close(slaveFd);
  //  }
  //  execlp("/bin/bash", "/bin/bash", (char *) NULL);
  //  errExit("execlp");
  //}

  //if(dup2(slaveFd, STDIN_FILENO) != STDIN_FILENO) {
  //  errExit("dup2 stdin");
  //}
  //if(dup2(slaveFd, STDOUT_FILENO) != STDOUT_FILENO) {
  //  errExit("dup2 stdout");
  //}
  //if(dup2(slaveFd, STDERR_FILENO) != STDERR_FILENO) {
  //  errExit("dup2 stderr");
  //}
  while(1) {
    int numRead;
    if((numRead = read(STDIN_FILENO, buf, BUF_SIZE) == -1)) {
        errExit("read");
    }
    fprintf(stdout, "%s\n", buf);

    if(write(masterFd, buf, numRead) != numRead) {
      errExit("write");
    }
    printf("ok");
  }
  return 0;
}
