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
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <nl.h>

#define NLMSG_TAIL(nmsg) \
        ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

extern size_t nlmsg_len(const struct nlmsg *nlmsg)
{
	return nlmsg->nlmsghdr.nlmsg_len - NLMSG_HDRLEN;
}

extern void *nlmsg_data(struct nlmsg *nlmsg)
{
	char *data = ((char *)nlmsg) + NLMSG_ALIGN(sizeof(struct nlmsghdr));
	if (!nlmsg_len(nlmsg))
		return NULL;
	return data;
}

static int nla_put(struct nlmsg *nlmsg, int attr, 
		   const void *data, size_t len)
{
  // rooting attribute
	struct rtattr *rta;
	size_t rtalen = RTA_LENGTH(len);
	
        rta = NLMSG_TAIL(&nlmsg->nlmsghdr);
        rta->rta_type = attr;
        rta->rta_len = rtalen;
        // returns a pointer to the start of this attribute's data.
        memcpy(RTA_DATA(rta), data, len);
        // NLMSG_ALIGN: Round the length of a netlink message up to align it properly.
        nlmsg->nlmsghdr.nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsghdr.nlmsg_len) + RTA_ALIGN(rtalen);
	return 0;
}

extern int nla_put_buffer(struct nlmsg *nlmsg, int attr, 
			  const void *data, size_t size)
{
	return nla_put(nlmsg, attr, data, size);
}

extern int nla_put_string(struct nlmsg *nlmsg, int attr, const char *string)
{
        return nla_put(nlmsg, attr, string, strlen(string) + 1);
}

extern int nla_put_u32(struct nlmsg *nlmsg, int attr, int value)
{
	return nla_put(nlmsg, attr, &value, sizeof(value));
}

extern int nla_put_u16(struct nlmsg *nlmsg, int attr, ushort value)
{
	return nla_put(nlmsg, attr, &value, 2);
}

extern int nla_put_attr(struct nlmsg *nlmsg, int attr)
{
  // static int nla_put(struct nlmsg *nlmsg, int attr, const void *data, size_t len)
	return nla_put(nlmsg, attr, NULL, 0);
}

struct rtattr *nla_begin_nested(struct nlmsg *nlmsg, int attr)
{
  // nmsg after the length of netlink
	struct rtattr *rtattr = NLMSG_TAIL(&nlmsg->nlmsghdr);

	if (nla_put_attr(nlmsg, attr))
		return NULL;

	return rtattr;
}

void nla_end_nested(struct nlmsg *nlmsg, struct rtattr *attr)
{
	attr->rta_len = (void *)NLMSG_TAIL(&nlmsg->nlmsghdr) - (void *)attr;
}

extern struct nlmsg *nlmsg_alloc(size_t size)
{
	struct nlmsg *nlmsg;
  // 8KB
  // Round the length of a netlink message up to align it properly.
	size_t len = NLMSG_ALIGN(size) + NLMSG_ALIGN(sizeof(struct nlmsghdr *));

	nlmsg = (struct nlmsg *)malloc(len);
	if (!nlmsg)
		return NULL;

	memset(nlmsg, 0, len);
	nlmsg->nlmsghdr.nlmsg_len = NLMSG_ALIGN(size);

	return nlmsg;
}

extern void nlmsg_free(struct nlmsg *nlmsg)
{
	free(nlmsg);
}

extern int netlink_rcv(struct nl_handler *handler, struct nlmsg *answer)
{
	int ret;
        struct sockaddr_nl nladdr;
        struct iovec iov = {
                .iov_base = answer,
                .iov_len = answer->nlmsghdr.nlmsg_len,
        };
	
	struct msghdr msg = {
                .msg_name = &nladdr,
                .msg_namelen = sizeof(nladdr),
                .msg_iov = &iov,
                .msg_iovlen = 1,
        };
	
        memset(&nladdr, 0, sizeof(nladdr));
        nladdr.nl_family = AF_NETLINK;
        nladdr.nl_pid = 0;
        nladdr.nl_groups = 0;

again:
  // ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
	ret = recvmsg(handler->fd, &msg, 0);
	if (ret < 0) {
		if (errno == EINTR)
			goto again;
		return -errno;
	}

	if (!ret)
		return 0;

	if (msg.msg_flags & MSG_TRUNC &&
	    ret == answer->nlmsghdr.nlmsg_len)
		return -EMSGSIZE;

	return ret;
}

// ret = netlink_send(handler, request);
extern int netlink_send(struct nl_handler *handler, struct nlmsg *nlmsg)
{
        struct sockaddr_nl nladdr;
        /*
          struct iovec {
              void  *iov_base;    // Starting address
              size_t iov_len;     // Number of bytes to transfer
          };
         */
        struct iovec iov = {
                .iov_base = (void*)nlmsg,
                .iov_len = nlmsg->nlmsghdr.nlmsg_len,
        };
	struct msghdr msg = {
                .msg_name = &nladdr,
                .msg_namelen = sizeof(nladdr),
                .msg_iov = &iov,
                .msg_iovlen = 1,
                // no need to auxliriary data
        };
	int ret;
	
        memset(&nladdr, 0, sizeof(nladdr));
        nladdr.nl_family = AF_NETLINK;
        nladdr.nl_pid = 0;
        nladdr.nl_groups = 0;

  // ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
  // ssize_t sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);
  // equivalent to sendto(handler->fd, (void*)nlmsg, nlmsg->nlmsghdr.nlmsg_len, 0, &nladdr, sizeof(struct sockaddr_nl));
	ret = sendmsg(handler->fd, &msg, 0);
	if (ret < 0)
		return -errno;

	return ret;
}

#ifndef NLMSG_ERROR
#define NLMSG_ERROR                0x2
#endif
extern int netlink_transaction(struct nl_handler *handler, 
            // nlmsg = nlmsghdr
			       struct nlmsg *request, struct nlmsg *answer)
{
	int ret;

	ret = netlink_send(handler, request);
	if (ret < 0)
		return ret;

	ret = netlink_rcv(handler, answer);
	if (ret < 0)
		return ret;

	if (answer->nlmsghdr.nlmsg_type == NLMSG_ERROR) {
    // nlmsg
    //  After each nlmsghdr the payload follows.
    // NLMSG_ERROR message signals an error and the payload contains an nlmsgerr structure
		struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(answer);
		return err->error;
	}

	return 0;
}

// If you use rtnetlink, set `NETLINK_ROUTE` flag.
// 	err = netlink_open(&nlh, NETLINK_ROUTE);
extern int netlink_open(struct nl_handler *handler, int protocol)
{
        socklen_t socklen;
        int sndbuf = 32768;
        int rcvbuf = 32768;

        memset(handler, 0, sizeof(*handler));

        // set AF_NETLINK.
        // Both SOCK_RAW and SOCK_DGRAM are valid values for socket_type
        handler->fd = socket(AF_NETLINK, SOCK_RAW, protocol);
        if (handler->fd < 0)
                return -errno;

        // setsockopt is similar to `int fcntl(int fd, int cmd, ... /* arg */ );`
        // int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
        // SOL_SOCKET : SOCKET layer
        if (setsockopt(handler->fd, SOL_SOCKET, SO_SNDBUF, 
		       &sndbuf, sizeof(sndbuf)) < 0)
                return -errno;

        if (setsockopt(handler->fd, SOL_SOCKET, SO_RCVBUF, 
		       &rcvbuf,sizeof(rcvbuf)) < 0)
                return -errno;

        /*
        struct nl_handler {
        int fd;
        int seq; // the sequence number of the netlink messages. This is identified by `time`
        struct sockaddr_nl local;
        struct sockaddr_nl peer; }; */
        /*
      struct sockaddr_nl {
          sa_family_t     nl_family;  // AF_NETLINK
          unsigned short  nl_pad;     // always 0
          pid_t           nl_pid;     // port ID(if kernel, always set zero)
          __u32           nl_groups;  // multi cast group mask(if unicast, set 0)
      };
         */
        // usually sockaddr_un(unix domain socket)
        // The sockaddr_nl structure describes a netlink client in user space or in the kernel
        memset(&handler->local, 0, sizeof(handler->local));
        handler->local.nl_family = AF_NETLINK;
        handler->local.nl_groups = 0;

        // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
        // bind socket file descriptor to the address(sockaddr_nl)
        if (bind(handler->fd, (struct sockaddr*)&handler->local, sizeof(handler->local)) < 0)
          return -errno;
        socklen = sizeof(handler->local);
        // int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
        if (getsockname(handler->fd, (struct sockaddr*)&handler->local, 
			&socklen) < 0)
                return -errno;

        if (socklen != sizeof(handler->local))
                return -EINVAL;

        if (handler->local.nl_family != AF_NETLINK)
                return -EINVAL;

	handler->seq = time(NULL);

        return 0;
}

extern int netlink_close(struct nl_handler *handler)
{
	close(handler->fd);
	handler->fd = -1;
	return 0;
}

