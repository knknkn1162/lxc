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
//#include <linux/sockios.h>
//#include <linux/if_bridge.h>
#include <time.h>

//#define IFLA_LINKMODE 17
//#define IFLA_LINKINFO 18
//#define IFLA_NET_NS_PID 19
//#define IFLA_INFO_KIND 1
//#define IFLA_VLAN_ID 1
//#define IFLA_INFO_DATA 2
//#define VETH_INFO_PEER 1
//#define IFLA_MACVLAN_MODE 1

#define PAGE_SIZE 4096
#define NLMSG_GOOD_SIZE (2*PAGE_SIZE)
// #define NLMSG_TAIL(nmsg) \
//        ((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))
#define errExit(msg) do { perror(msg); exit(EXIT_FAILURE); } while (0)

struct nl_handler {
  int fd;
  int seq;
  struct sockaddr_nl local;
  struct sockaddr_nl peer;
};
/*
struct sockaddr_nl {
    sa_family_t nl_family; unsigned short nl_pad;    pid_t nl_pid; __u32 nl_groups; }; */

struct link_req { struct nlmsghdr request; struct ifinfomsg ifinfomsg; };

// nlmsg_len: whole length including header
// struct nlmsghdr { __u32 nlmsg_len; __u16 nlmsg_type; __u16 nlmsg_flags; __u32 nlmsg_seq; __u32 nlmsg_pid; };
/* struct ifinfomsg { unsigned char ifi_family; unsigned short ifi_type; int ifi_index; unsigned int ifi_flags; unsigned int ifi_change; }; */

int veth_create(const char *name1, const char *name2);

void *nlmsg_data(struct nlmsghdr *request) {
  // data section
  char *data = ((char *)request) + NLMSG_ALIGN(sizeof(struct nlmsghdr));
  // data
  if(request->nlmsg_len - NLMSG_HDRLEN == 0) {
    return NULL;
  }
  return data;
}

// configure rooting attribute with request
static int nla_put(struct nlmsghdr *request, int attr, const void *data, size_t len) {
  // rooting attribute
  struct rtattr *rta;
  // whole len bytes of data plus the header.
  size_t rtalen = RTA_LENGTH(len);

  // struct rtattr { unsigned short rta_len; unsigned short rta_type; };
  rta = ((struct rtattr *) (((void *) (request)) + NLMSG_ALIGN(request->nlmsg_len)));
  rta->rta_type = attr;
  rta->rta_len = rtalen;
  // returns a pointer to the start of this attribute's data.
  // void *memcpy(void *dst, const void *src, size_t n);
  // a pointer to the start of this attribute's data.
  memcpy(RTA_DATA(rta), data, len);
  request->nlmsg_len = NLMSG_ALIGN(request->nlmsg_len) + RTA_ALIGN(rtalen);
  printf("nla_put: len -> %d\n", request->nlmsg_len);
	return 0;
}

int nla_put_buffer(struct nlmsghdr *request, int attr,  const void *data, size_t size) {
	return nla_put(request, attr, data, size);
}

int nla_put_string(struct nlmsghdr *request, int attr, const char *string) {
  return nla_put(request, attr, string, strlen(string) + 1);
}

//4 byte
int nla_put_u32(struct nlmsghdr *request, int attr, int value) {
	return nla_put(request, attr, &value, sizeof(value));
}

// 2byte
int nla_put_u16(struct nlmsghdr *request, int attr, ushort value) {
	return nla_put(request, attr, &value, 2);
}

// with no rta_data
int nla_put_attr(struct nlmsghdr *request, int attr) {
	return nla_put(request, attr, NULL, 0);
}

struct rtattr *nla_begin_nested(struct nlmsghdr *request, int attr)
{
  struct rtattr *rtattr = (struct rtattr *)(((void *) (request)) + NLMSG_ALIGN(request->nlmsg_len));
	if (nla_put_attr(request, attr))
		return NULL;

	return rtattr;
}

void nla_end_nested(struct nlmsghdr *request, struct rtattr *attr)
{
  // Length of option
  attr->rta_len = (void*)(((void *) (request)) + NLMSG_ALIGN((request)->nlmsg_len)) - (void*)attr;
	//attr->rta_len = (void *)NLMSG_TAIL(&nlmsg->request) - (void *)attr;
}

// nlmsg_alloc(NLMSG_GOOD_SIZE); NLMSG_GOOD_SIZE = 8KB
struct nlmsghdr *nlmsg_alloc(size_t size)
{
	struct nlmsghdr *request;
  // 8KB
  // NLMSG_ALIGN: Round the length of a netlink message up to align it properly.
	size_t len = NLMSG_ALIGN(size) + NLMSG_ALIGN(sizeof(struct nlmsghdr));

  if((request = (struct nlmsghdr *)malloc(len)) == NULL) {
    return NULL;
  }

	memset(request, 0, len);
	request->nlmsg_len = NLMSG_ALIGN(size);

	return request;
}

void nlmsg_free(struct nlmsghdr *request) {
  free(request);
}

int netlink_rcv(struct nl_handler *handler, struct nlmsghdr *answer)
{
  int ret;
  struct sockaddr_nl nladdr;
  struct iovec iov = { .iov_base = answer, .iov_len = answer->nlmsg_len };
  
/*
struct msghdr {
    void *msg_name; socklen_t msg_namelen; struct iovec *msg_iov; size_t msg_iovlen; void *msg_control; size_t msg_controllen; int msg_flags; };
 */
  struct msghdr msg = { .msg_name = &nladdr, .msg_namelen = sizeof(nladdr), .msg_iov = &iov, .msg_iovlen = 1, };

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  nladdr.nl_pid = 0;
  nladdr.nl_groups = 0;

again:
  // ssize_t recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);
  if(ret = recvmsg(handler->fd, &msg, 0) < 0) {
    if (errno == EINTR)
      goto again;
    return -errno;
  // 
  } else if (ret == 0) {
    printf("after recvmsg:\n");
    printf("struct nlmsghdr(answer): len: %d, type: %d, pid: %d, seq: %d\n",
      answer->nlmsg_len, answer->nlmsg_type, answer->nlmsg_pid, answer->nlmsg_seq
    );
    return 0;
  }

  // the buffer is too small to truncate(but msg_flags is unused..)
  if (msg.msg_flags & MSG_TRUNC && ret == answer->nlmsg_len)
    return -EMSGSIZE;

  // after return, check answer->nlmsg_type == NLMSG_ERROR
  return ret;
}

// ret = netlink_send(handler, request);
int netlink_send(struct nl_handler *handler, struct nlmsghdr *request)
{
  struct sockaddr_nl nladdr;
  struct iovec iov = { .iov_base = (void*)request, .iov_len = request->nlmsg_len, };
  struct msghdr msg = { .msg_name = &nladdr, .msg_namelen = sizeof(nladdr), .msg_iov = &iov, .msg_iovlen = 1, };
  int ret;

  memset(&nladdr, 0, sizeof(nladdr));
  nladdr.nl_family = AF_NETLINK;
  nladdr.nl_pid = 0;
  nladdr.nl_groups = 0;

  // ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
  // handler->fd is derived from `handler->fd = socket(AF_NETLINK, SOCK_RAW, protocol);`
  if((ret = sendmsg(handler->fd, &msg, 0)) < 0) {
    return -errno;
  } else {
    return ret;
  }
}

// NLMSG_ERROR is declared in `include/uapi/linux/netlink.h`
//#define NLMSG_ERROR                0x2
// err = netlink_transaction(&nlh, request, answer);
int netlink_transaction(struct nl_handler *handler, struct nlmsghdr *request, struct nlmsghdr *answer)
{
  int ret;

  ret = netlink_send(handler, request);
  printf("sendmsg OK\n");
  if (ret < 0)
    return ret;

  ret = netlink_rcv(handler, answer);
  if (ret < 0)
    return ret;
  if (answer->nlmsg_type == NLMSG_ERROR) {
    // nlmsg
    //  After each request the payload follows.
    // NLMSG_ERROR message signals an error and the payload contains an nlmsgerr structure
    struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(answer);
    printf("NLMGS_ERROR: %d\n", err->error);
    return err->error;
  }
  printf("recvmsg OK\n");
  return 0;
}

// If you use rtnetlink, set `NETLINK_ROUTE` flag.
// err = netlink_open(&nlh, NETLINK_ROUTE);
int netlink_open(struct nl_handler *handler, int protocol)
{
  socklen_t socklen;
  int sndbuf = 32768;
  int rcvbuf = 32768;

  memset(handler, 0, sizeof(*handler));

  // set AF_NETLINK.
  // Both SOCK_RAW and SOCK_DGRAM are valid values for socket_type
  // datagrams (connectionless, unreliable messages of a fixed maximum length)
  handler->fd = socket(AF_NETLINK, SOCK_RAW, protocol);
  if (handler->fd < 0)
    return -errno;

  // setsockopt is similar to `int fcntl(int fd, int cmd, ... /* arg */ );`
  // int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
  // SOL_SOCKET : SOCKET layer
  if (setsockopt(handler->fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0)
    return -errno;
  if (setsockopt(handler->fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf,sizeof(rcvbuf)) < 0)
    return -errno;

  // usually sockaddr_un(unix domain socket)
  // The sockaddr_nl structure describes a netlink client in user space or in the kernel
  memset(&handler->local, 0, sizeof(struct sockaddr_nl));
  handler->local.nl_family = AF_NETLINK;
  handler->local.nl_groups = 0;

  // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
  // bind socket file descriptor to the address(sockaddr_nl)
  if (bind(handler->fd, (struct sockaddr*)&handler->local, sizeof(handler->local)) < 0)
    return -errno;
  socklen = sizeof(handler->local);

  // check
  // // int getsockname(int sockfd, struct sockaddr *addr, socklen_t *addrlen); // socklen_t is equivalent to int*
  if (getsockname(handler->fd, (struct sockaddr*)&handler->local, &socklen) < 0)
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


// require sudo
int main(void) {
  if(veth_create("veth0", "veth1")) {
    errExit("veth_create");
  }
  printf("create peer veth0 and veth1\n");
  return 0;
}

int veth_create(const char *name1, const char *name2)
{
  // struct nl_handler { int fd; int seq; struct sockaddr_nl local; struct sockaddr_nl peer; };
  struct nl_handler nlh;
  struct nlmsghdr *request = NULL, *answer = NULL;
  struct link_req *link_req;
  // route table attribute
  // struct rtattr { unsigned short rta_len; unsigned short rta_type; };
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
  if((request = nlmsg_alloc(NLMSG_GOOD_SIZE)) == NULL) {
    goto out;
  }

  // struct link_req { struct nlmsghdr request; struct ifinfomsg ifinfomsg; };
  link_req = (struct link_req *)request;
  // AF_UNSPEC is declared in `linux/include/linux/socket.h`
  link_req->ifinfomsg.ifi_family = AF_UNSPEC;
  request->nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));

  // NLM_F_REQUEST: Must be set on all request messages.
  // additional flag bits
  // NLM_F_CREATE: Create object if it doesn't already exist.
  // NLM_F_EXCL: Don't replace if the object already exists.
  // NLM_F_ACK: Request for an acknowledgment on success.
  request->nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
  // ip link add
  request->nlmsg_type = RTM_NEWLINK;

	err = -EINVAL;
  /*
 *   [IFLA_LINKINFO] = {
*      [AF_INFO_KIND] = "veth",
*      [IFLA_INFO_DATA] = {
*        [VETH_INFO_PEER] = {
*          [IFLA_IFNAME] = ${veth02}
*        },
*      },
 *   },
 *   [IFLA_IFNAME] = ${veth01}
   */
  // nla_put .. set rta at the tail of the request and set rta->type=IFLA_LINKINFO & update nlmsg_hdr->nlmsg_len
	nest1 = nla_begin_nested(request, IFLA_LINKINFO);
	if (!nest1)
		goto out;

  // rta->rta_type = INFA_INFO_KIND;
  // memcpy(RTA_DATA(rta), "veth", len);
	if (nla_put_string(request, IFLA_INFO_KIND, "veth"))
		goto out;

  // #  define IFLA_INFO_DATA 2
	nest2 = nla_begin_nested(request, IFLA_INFO_DATA);
	if (!nest2)
		goto out;

  // VETH_INFO_PEER(1) is defined in ./linux/include/uapi/linux/veth.h
	nest3 = nla_begin_nested(request, VETH_INFO_PEER);
	if (!nest3)
		goto out;

	request->nlmsg_len += sizeof(struct ifinfomsg);

  // extern int nla_put_string(struct nlmsg *nlmsg, int attr, const char *string)
  // rta->rta_type = IFLA_IFNAME // Device name.
  // memcpy(RTA_DATA(rta), name2, len);
	if (nla_put_string(request, IFLA_IFNAME, name2))
		goto out;

  // void nla_end_nested(struct nlmsghdr *request, struct rtattr *attr)
  // update length in attr->rta_len
	nla_end_nested(request, nest3);

	nla_end_nested(request, nest2);

	nla_end_nested(request, nest1);

	if (nla_put_string(request, IFLA_IFNAME, name1))
		goto out;

  // extern int netlink_transaction(struct nl_handler *handler, struct nlmsg *request, struct nlmsg *answer)

  // use netlink_transaction
  answer = nlmsg_alloc(NLMSG_GOOD_SIZE);
  if (!answer)
    goto out;
  // struct nl_handler
	err = netlink_transaction(&nlh, request, answer);
out:
	netlink_close(&nlh);
	nlmsg_free(answer);
	nlmsg_free(request);
	return err;
}
