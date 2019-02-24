/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 * Daniel Lezcano <dlezcano at fr.ibm.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#define _GNU_SOURCE
#include <stdio.h>
#undef _GNU_SOURCE
#include <stdlib.h>
#include <errno.h>
#include <mntent.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/inotify.h>
#include <netinet/in.h>
#include <net/if.h>

#include "error.h"
#include "config.h"

#include <lxc/log.h>
#include <lxc/cgroup.h>
#include <lxc/start.h>

lxc_log_define(lxc_cgroup, lxc);

#define MTAB "/proc/mounts"

static char nsgroup_path[MAXPATHLEN];

enum {
	CGROUP_NS_CGROUP = 1,
	CGROUP_CLONE_CHILDREN,
};

// get_cgroup_mount(MTAB, cgmnt)
static int get_cgroup_mount(const char *mtab, char *mnt)
{
        struct mntent *mntent;
        FILE *file = NULL;
        int err = -1;

        // FILE *setmntent(const char *filename, const char *type);
        file = setmntent(mtab, "r");
        if (!file) {
                SYSERROR("failed to open %s", mtab);
		return -1;
        }

        while ((mntent = getmntent(file))) {

          /* there is a cgroup mounted named "lxc" */
          // if lxc is already exists, mnt <- mnt_dir
          if (!strcmp(mntent->mnt_fsname, "lxc") &&
              !strcmp(mntent->mnt_type, "cgroup")) {
            if(strstr(mntent->mnt_opts, "devices")) {
              strcpy(mnt, mntent->mnt_dir);
              err = 0;
              break;
            }
          }

          /* fallback to the first non-lxc cgroup found */
          // if mnt_type = cgroup
          // find first element that meets
          if (!strcmp(mntent->mnt_type, "cgroup") && err) {
            if(strstr(mntent->mnt_opts, "devices")) {
              // strcpy(dst, src);
              strcpy(mnt, mntent->mnt_dir);
              err = 0;
            }
          }
        };

  // using cgoupr mounted at /sys/fs/cgroup/systemd
	DEBUG("using cgroup mounted at '%s'", mnt);

        fclose(file);

        return err;
}

static int get_cgroup_flags(const char *mtab, int *flags)
{
        struct mntent *mntent;
        FILE *file = NULL;
        int err = -1;

        file = setmntent(mtab, "r");
        if (!file) {
                SYSERROR("failed to open %s", mtab);
		return -1;
        }

	*flags = 0;

        while ((mntent = getmntent(file))) {

		/* there is a cgroup mounted named "lxc" */
		if (!strcmp(mntent->mnt_fsname, "lxc") &&
		    !strcmp(mntent->mnt_type, "cgroup")) {

			if (hasmntopt(mntent, "ns"))
				*flags |= CGROUP_NS_CGROUP;

			if (hasmntopt(mntent, "clone_children"))
				*flags |= CGROUP_CLONE_CHILDREN;

			err = 0;
			break;
		}

		/* fallback to the first non-lxc cgroup found */
                if (!strcmp(mntent->mnt_type, "cgroup") && err) {

			if (hasmntopt(mntent, "ns"))
				*flags |= CGROUP_NS_CGROUP;

			if (hasmntopt(mntent, "clone_children"))
				*flags |= CGROUP_CLONE_CHILDREN;

			err = 0;
		}
        };

  // cgroup flags is 0x2
	DEBUG("cgroup flags is 0x%x", *flags);

        fclose(file);

        return err;
}

static int cgroup_rename_nsgroup(const char *mnt, const char *name, pid_t pid)
{
	char oldname[MAXPATHLEN];

	snprintf(oldname, MAXPATHLEN, "%s/%d", mnt, pid);

	if (rename(oldname, name)) {
		SYSERROR("failed to rename cgroup %s->%s", oldname, name);
		return -1;
	}

	DEBUG("'%s' renamed to '%s'", oldname, name);

	return 0;
}

static int cgroup_enable_clone_children(const char *path)
{
	FILE *f;
	int ret = 0;

	f = fopen(path, "w");
	if (!f) {
		SYSERROR("failed to open '%s'", path);
		return -1;
	}

	if (fprintf(f, "1") < 1) {
		ERROR("failed to write flag to '%s'", path);
		ret = -1;
	}

	fclose(f);

	return ret;
}

// 	/* Let's add the pid to the 'tasks' file */
static int cgroup_attach(const char *path, pid_t pid)
{
	FILE *f;
	char tasks[MAXPATHLEN];
	int ret = 0;

  // cgname : /sys/fs/cgroup/systemd/${container_name}/tasks
	snprintf(tasks, MAXPATHLEN, "%s/tasks", path);

	f = fopen(tasks, "w");
	if (!f) {
		SYSERROR("failed to open '%s'", tasks);
		return -1;
	}

	if (fprintf(f, "%d", pid) <= 0) {
		SYSERROR("failed to write pid '%d' to '%s'", pid, tasks);
		ret = -1;
	}

	fclose(f);

	return ret;
}

// pid: child_pid that clone generates
int lxc_cgroup_create(const char *name, pid_t pid)
{
	char cgmnt[MAXPATHLEN];
	char cgname[MAXPATHLEN];
	char clonechild[MAXPATHLEN];
	int flags;

  // #define MTAB "/proc/mounts", "/proc/self/mounts"
  // cgroup /sys/fs/cgroup/unified cgroup2 rw,nosuid,nodev,noexec,relatime 0 0
  // cgroup /sys/fs/cgroup/systemd cgroup rw,nosuid,nodev,noexec,relatime,xattr,name=systemd 0 0
  // cgroup /sys/fs/cgroup/cpuset cgroup rw,nosuid,nodev,noexec,relatime,cpuset 0 0
  // cgroup /sys/fs/cgroup/net_cls,net_prio cgroup rw,nosuid,nodev,noexec,relatime,net_cls,net_prio 0 0
  // cgroup /sys/fs/cgroup/freezer cgroup rw,nosuid,nodev,noexec,relatime,freezer 0 0
  // cgroup /sys/fs/cgroup/rdma cgroup rw,nosuid,nodev,noexec,relatime,rdma 0 0
  // cgroup /sys/fs/cgroup/perf_event cgroup rw,nosuid,nodev,noexec,relatime,perf_event 0 0
  // cgroup /sys/fs/cgroup/blkio cgroup rw,nosuid,nodev,noexec,relatime,blkio 0 0
  // cgroup /sys/fs/cgroup/memory cgroup rw,nosuid,nodev,noexec,relatime,memory 0 0
  // cgroup /sys/fs/cgroup/cpu,cpuacct cgroup rw,nosuid,nodev,noexec,relatime,cpu,cpuacct 0 0
  // cgroup /sys/fs/cgroup/hugetlb cgroup rw,nosuid,nodev,noexec,relatime,hugetlb 0 0
  // cgroup /sys/fs/cgroup/pids cgroup rw,nosuid,nodev,noexec,relatime,pids 0 0
  // cgroup /sys/fs/cgroup/devices cgroup rw,nosuid,nodev,noexec,relatime,devices 0 0
	if (get_cgroup_mount(MTAB, cgmnt)) {
		ERROR("cgroup is not mounted");
		return -1;
	}

  // cgname : /sys/fs/cgroup/systemd/${container_name}
	snprintf(cgname, MAXPATHLEN, "%s/%s", cgmnt, name);

	/*
	 * There is a previous cgroup, assume it is empty,
	 * otherwise that fails
	 */
	if (!access(cgname, F_OK) && rmdir(cgname)) {
		SYSERROR("failed to remove previous cgroup '%s'", cgname);
		return -1;
	}

  //   // cgroup flags is 0x2
  // rw,nosuid,nodev,noexec,relatime,xattr,name=systemd
	if (get_cgroup_flags(MTAB, &flags)) {
		SYSERROR("failed to get cgroup flags");
		return -1;
	}

	/* We have the deprecated ns_cgroup subsystem */
	if (flags & CGROUP_NS_CGROUP) {
		WARN("using deprecated ns_cgroup");
		return cgroup_rename_nsgroup(cgmnt, cgname, pid);
	}

	snprintf(clonechild, MAXPATHLEN, "%s/cgroup.clone_children", cgmnt);

	/* we check if the kernel has clone_children, at this point if there
	 * no clone_children neither ns_cgroup, that means the cgroup is mounted
	 * without the ns_cgroup and it has not the compatibility flag
	 */
	if (access(clonechild, F_OK)) {
		ERROR("no ns_cgroup option specified");
		return -1;
	}

	/* we enable the clone_children flag of the cgroup */
	if (cgroup_enable_clone_children(clonechild)) {
		SYSERROR("failed to enable 'clone_children flag");
		return -1;
	}

	/* Let's create the cgroup */
	if (mkdir(cgname, 0700)) {
		SYSERROR("failed to create '%s' directory", cgname);
		return -1;
	}

	/* Let's add the pid to the 'tasks' file */
  // detach process from systemd cgroup
	if (cgroup_attach(cgname, pid)) {
		SYSERROR("failed to attach pid '%d' to '%s'", pid, cgname);
		rmdir(cgname);
		return -1;
	}

	return 0;
}

int lxc_cgroup_destroy(const char *name)
{
	char cgmnt[MAXPATHLEN];
	char cgname[MAXPATHLEN];

	if (get_cgroup_mount(MTAB, cgmnt)) {
		ERROR("cgroup is not mounted");
		return -1;
	}

	snprintf(cgname, MAXPATHLEN, "%s/%s", cgmnt, name);
	if (rmdir(cgname)) {
		SYSERROR("failed to remove cgroup '%s'", cgname);
		return -1;
	}

	DEBUG("'%s' unlinked", cgname);

	return 0;
}

int lxc_cgroup_path_get(char **path, const char *name)
{
	char cgroup[MAXPATHLEN];

	*path = &nsgroup_path[0];

	/*
	 * report nsgroup_path string if already set
	 */
	if (**path != 0)
		return 0;

	if (get_cgroup_mount(MTAB, cgroup)) {
		ERROR("cgroup is not mounted");
		return -1;
	}

  // cgroup : /sys/fs/cgroup/systemd/${container_name}
	snprintf(nsgroup_path, MAXPATHLEN, "%s/%s", cgroup, name);
	return 0;
}

// lxc_cgroup_set(name, cg->subsystem, cg->value) where `struct lxc_cgroup *cg;`
int lxc_cgroup_set(const char *name, const char *subsystem, const char *value)
{
	int fd, ret;
	char *nsgroup;
	char path[MAXPATHLEN];

  // put ${rootfs}/sys/fs/cgroup/systemd/debian01 in `name` variable
	ret = lxc_cgroup_path_get(&nsgroup, name);
	if (ret)
		return -1;

    // path=`${rootfs}/sys/fs/cgroup/systemd/debian01/devices.deny`
    snprintf(path, MAXPATHLEN, "%s/%s", nsgroup, subsystem);

	fd = open(path, O_WRONLY);
	if (fd < 0) {
		ERROR("open %s : %s", path, strerror(errno));
		return -1;
	}

	ret = write(fd, value, strlen(value));
	if (ret < 0) {
		ERROR("write %s : %s", path, strerror(errno));
		goto out;
	}
	
	ret = 0;
out:
	close(fd);
	return ret;
}

int lxc_cgroup_get(const char *name, const char *subsystem,  
		   char *value, size_t len)
{
	int fd, ret = -1;
	char *nsgroup;
	char path[MAXPATHLEN];

	ret = lxc_cgroup_path_get(&nsgroup, name);
	if (ret)
		return -1;

        snprintf(path, MAXPATHLEN, "%s/%s", nsgroup, subsystem);

	fd = open(path, O_RDONLY);
	if (fd < 0) {
		ERROR("open %s : %s", path, strerror(errno));
		return -1;
	}

	ret = read(fd, value, len);
	if (ret < 0)
		ERROR("read %s : %s", path, strerror(errno));

	close(fd);
	return ret;
}

int lxc_cgroup_nrtasks(const char *name)
{
	char *nsgroup;
	char path[MAXPATHLEN];
	int pid, ret, count = 0;
	FILE *file;

	ret = lxc_cgroup_path_get(&nsgroup, name);
	if (ret)
		return -1;

        snprintf(path, MAXPATHLEN, "%s/tasks", nsgroup);

	file = fopen(path, "r");
	if (!file) {
		SYSERROR("fopen '%s' failed", path);
		return -1;
	}

	while (fscanf(file, "%d", &pid) != EOF)
		count++;

	fclose(file);

	return count;
}
