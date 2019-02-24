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
#include <syslog.h>
#include <mntent.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

int main(void) {
  struct mntent *mntent;
  FILE* fp;

  if(!(fp = setmntent("/proc/self/mounts", "r"))) {
    errExit("setmntent");
  }

  //struct mntent {
  //    char *mnt_fsname;   /* name of mounted file system */
  //    char *mnt_dir;      /* file system path prefix */
  //    char *mnt_type;     /* mount type (see mntent.h) */
  //    char *mnt_opts;     /* mount options (see mntent.h) */
  //    int   mnt_freq;     /* dump frequency in days */
  //    int   mnt_passno;   /* pass number on parallel fsck */
  //};
  while((mntent = getmntent(fp))) {
    printf("mnt_fsname: %s\n", mntent->mnt_fsname);
    printf("mnt_dir: %s\n", mntent->mnt_dir);
    printf("mnt_type: %s\n", mntent->mnt_type);
    printf("mnt_opts: %s\n", mntent->mnt_opts);
    printf("----------\n");
  }
  fclose(fp);
  return 0;
}
