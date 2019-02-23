#define _GNU_SOURCE
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/wait.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

/*
vagrant@ubuntu-bionic:~/shared/lxc/src/test$ ./a.out
setsid: Operation not permitted
vagrant@ubuntu-bionic:~/shared/lxc/src/test$ sudo ./a.out
bash: cannot set terminal process group (-1): Inappropriate ioctl for device
bash: no job control in this shell
$ ps_watch
vagrant  12714  0.0  0.2  23232  5176 pts/0    Ss   05:41   0:00 -bash
root     27662  0.0  0.2  63972  4208 pts/0    S+   06:05   0:00  \_ sudo ./a.out
*/
int main(void) {
  if(setsid() == -1) {
    errExit("setsid");
  }
  execlp("/bin/bash", "/bin/bash", "-i", (char*)NULL);
  // never reaches
  perror("execlp");
  return 0;
}
