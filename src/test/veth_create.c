#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <netinet/in.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/sockios.h>
#include <linux/if_bridge.h>
#include <time.h>

#define IFLA_LINKMODE 17
//#define IFLA_LINKINFO 18
#define IFLA_NET_NS_PID 19
#define IFLA_INFO_KIND 1
#define IFLA_VLAN_ID 1
#define IFLA_INFO_DATA 2
#define VETH_INFO_PEER 1
#define IFLA_MACVLAN_MODE 1

#define PAGE_SIZE 4096
#define NLMSG_GOOD_SIZE (2*PAGE_SIZE)
#define NLMSG_TAIL(nmsg) \
        ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

struct nl_handler {
  int fd;
  int seq;
  struct sockaddr_nl local;
  struct sockaddr_nl peer;
};
/*
struct sockaddr_nl {
    sa_family_t nl_family; unsigned short nl_pad;    pid_t nl_pid; __u32 nl_groups; }; */

struct nlmsg {
	struct nlmsghdr nlmsghdr;
};
struct link_req {
	struct nlmsg nlmsg;
	struct ifinfomsg ifinfomsg;
};
/* struct ifinfomsg { unsigned char ifi_family; unsigned short ifi_type; int ifi_index; unsigned int ifi_flags; unsigned int ifi_change; }; */

int lxc_veth_create(const char *name1, const char *name2);

size_t nlmsg_len(const struct nlmsg *nlmsg)
{
	return nlmsg->nlmsghdr.nlmsg_len - NLMSG_HDRLEN;
}

void *nlmsg_data(struct nlmsg *nlmsg)
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

int nla_put_buffer(struct nlmsg *nlmsg, int attr, 
			  const void *data, size_t size)
{
	return nla_put(nlmsg, attr, data, size);
}

int nla_put_string(struct nlmsg *nlmsg, int attr, const char *string)
{
        return nla_put(nlmsg, attr, string, strlen(string) + 1);
}

int nla_put_u32(struct nlmsg *nlmsg, int attr, int value)
{
	return nla_put(nlmsg, attr, &value, sizeof(value));
}

int nla_put_u16(struct nlmsg *nlmsg, int attr, ushort value)
{
	return nla_put(nlmsg, attr, &value, 2);
}

int nla_put_attr(struct nlmsg *nlmsg, int attr)
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

struct nlmsg *nlmsg_alloc(size_t size)
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

void nlmsg_free(struct nlmsg *nlmsg)
{
	free(nlmsg);
}

int netlink_rcv(struct nl_handler *handler, struct nlmsg *answer)
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
int netlink_send(struct nl_handler *handler, struct nlmsg *nlmsg)
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
int netlink_transaction(struct nl_handler *handler, 
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
int netlink_open(struct nl_handler *handler, int protocol)
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

int netlink_close(struct nl_handler *handler)
{
	close(handler->fd);
	handler->fd = -1;
	return 0;
}


int main(void) {
  lxc_veth_create("veth0", "veth1");
}

int lxc_veth_create(const char *name1, const char *name2)
{
  struct nl_handler nlh;
  struct nlmsg *nlmsg = NULL, *answer = NULL;
  struct link_req *link_req;
  struct rtattr *nest1, *nest2, *nest3;
  int len, err;

  // extern int netlink_open(struct nl_handler *handler, int protocol)
  // use socket, bind, getsockname
  // nlh->fd .. socket
  err = netlink_open(&nlh, NETLINK_ROUTE);
  if (err)
    return err;

  err = -EINVAL;
  // nla_put_string(nlmsg, IFLA_IFNAME, name1)
  len = strlen(name1);
  if (len == 1 || len > IFNAMSIZ)
    goto out;

  len = strlen(name2);
  if (len == 1 || len > IFNAMSIZ)
    goto out;

  err = -ENOMEM;
  nlmsg = nlmsg_alloc(NLMSG_GOOD_SIZE);
  if (!nlmsg)
    goto out;

  // use netlink_transaction
  answer = nlmsg_alloc(NLMSG_GOOD_SIZE);
  if (!answer)
    goto out;

  link_req = (struct link_req *)nlmsg;
  // 
  link_req->ifinfomsg.ifi_family = AF_UNSPEC;
  nlmsg->nlmsghdr.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
  // NLM_F_REQUEST: Must be set on all request messages.
  // additional flag bits
  // NLM_F_CREATE: Create object if it doesn't already exist.
  // NLM_F_EXCL: Don't replace if the object already exists.
  // NLM_F_ACK: Request for an acknowledgment on success.
	nlmsg->nlmsghdr.nlmsg_flags =
		NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
  // create network link
	nlmsg->nlmsghdr.nlmsg_type = RTM_NEWLINK;

	err = -EINVAL;
  /*
   * the structure of nlmsg
    IFLA_LINKINFO(18):
      - IFLA_INFO_KIND(1): veth
      IFLA_INFO_DATA(2):
        VETH_INFO_PEER(1):
          IFLA_IFNAME: ${name2}
    IFLA_IFNAME: ${name1}
   */
  // struct rtattr *nla_begin_nested(struct nlmsg *nlmsg, int attr)
  // rtattr is optional attributes after the initial header:
	nest1 = nla_begin_nested(nlmsg, IFLA_LINKINFO);
	if (!nest1)
		goto out;

  // rta->rta_type = INFA_INFO_KIND;
  // memcpy(RTA_DATA(rta), "veth", len);
	if (nla_put_string(nlmsg, IFLA_INFO_KIND, "veth"))
		goto out;

  // #  define IFLA_INFO_DATA 2
	nest2 = nla_begin_nested(nlmsg, IFLA_INFO_DATA);
	if (!nest2)
		goto out;

	nest3 = nla_begin_nested(nlmsg, VETH_INFO_PEER);
	if (!nest3)
		goto out;

	nlmsg->nlmsghdr.nlmsg_len += sizeof(struct ifinfomsg);

  // extern int nla_put_string(struct nlmsg *nlmsg, int attr, const char *string)
  // rta->rta_type = IFLA_IFNAME // Device name.
  // memcpy(RTA_DATA(rta), name2, len);
	if (nla_put_string(nlmsg, IFLA_IFNAME, name2))
		goto out;

	nla_end_nested(nlmsg, nest3);

	nla_end_nested(nlmsg, nest2);

	nla_end_nested(nlmsg, nest1);

	if (nla_put_string(nlmsg, IFLA_IFNAME, name1))
		goto out;

  // extern int netlink_transaction(struct nl_handler *handler, struct nlmsg *request, struct nlmsg *answer)
	err = netlink_transaction(&nlh, nlmsg, answer);
out:
	netlink_close(&nlh);
	nlmsg_free(answer);
	nlmsg_free(nlmsg);
	return err;
}
