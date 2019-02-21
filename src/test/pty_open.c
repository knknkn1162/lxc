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

/* A simple error-handling function: print an error message based
   on the value in 'errno' and terminate the calling process */

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

struct pty_info {
  char name[MAXPATHLEN]; int master_fd; int slave_fd; int busy;
};

int main(void) {
  struct pty_info pty_info;
  openpty(&pty_info.master_fd, &pty_info.slave_fd, pty_info.name, NULL, NULL);

  // get the name of slave device
  printf("allocated pty '%s' (%d/%d)\n", pty_info.name, pty_info.master_fd, pty_info.slave_fd);

  /* Prevent leaking the file descriptors to the container */
  fcntl(pty_info.master_fd, F_SETFD, FD_CLOEXEC);
  fcntl(pty_info.slave_fd, F_SETFD, FD_CLOEXEC);
  sleep(60);
  return 0;
}
/*
```
vagrant@ubuntu-bionic:~/shared/src/test$ ./a.out
allocated pty '/dev/pts/2' (3/4)
^Z
[2]+  Stopped                 ./a.out
vagrant@ubuntu-bionic:~/shared/src/test$ ./a.out
allocated pty '/dev/pts/3' (3/4)
^Z
[3]+  Stopped                 ./a.out
vagrant@ubuntu-bionic:~/shared/src/test$ ./a.out
allocated pty '/dev/pts/4' (3/4)
^Z
[4]+  Stopped                 ./a.out
vagrant@ubuntu-bionic:~/shared/src/test$ ./a.out
allocated pty '/dev/pts/5' (3/4)
^Z
[5]+  Stopped                 ./a.out
ls -ls /dev/pts
0 crw--w---- 1 vagrant tty  136, 0 Feb 21 03:11 0
0 crw--w---- 1 vagrant tty  136, 1 Feb 21 03:06 1
0 crw--w---- 1 vagrant tty  136, 2 Feb 21 03:10 2
0 crw--w---- 1 vagrant tty  136, 3 Feb 21 03:10 3
0 crw--w---- 1 vagrant tty  136, 4 Feb 21 03:10 4
0 crw--w---- 1 vagrant tty  136, 5 Feb 21 03:10 5
```
*/
