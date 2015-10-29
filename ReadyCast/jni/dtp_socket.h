/* dtp_socket.h -- TurtleFlow socket API */

#ifndef __DTP_SOCKET_H__
#define __DTP_SOCKET_H__

#include <arpa/inet.h>
#include <stdbool.h>
#include <poll.h>

#include "dtp_select.h"

/* dhkim, add for compatibility */
#define IPPROTO_DTP 253
#if 0
#define DPROX_IP	"143.248.53.65"
#define DPROX_IP_HEX 0x4135F88F
#define DPROX_PORT	3742
#define DPROX_ADDR	"codepaint.kaist.ac.kr"
#else
#define DPROX_IP	"203.255.250.0"
#define DPROX_IP_HEX 0xFCFAFFCB
#define DPROX_PORT	9000
#endif

typedef int dtp_socket_t;

extern int
dtp_getsockname(dtp_socket_t socket, struct sockaddr *addr, socklen_t *len);

/*
 *  dtp_getpeername() - get the name of the peer socket.
 *
 *  socket: context socket.
 *  addr: socket address.
 *  len: socket address length.
 */
extern int
dtp_getpeername(dtp_socket_t socket, struct sockaddr *addr, socklen_t *len);

/*
 *  dtp_isdtpsocket() - returns true if the socket is dtp_socket
 */
extern bool
dtp_isdtpsocket(dtp_socket_t socket);

/*
 *  dtp_socket() - Create a DTP library thread. 
 *                Create a new UDP socket fd. 
 *                Create a new DTP context for connect.
 */
extern int 
dtp_socket(void);

/*
 *  dtp_bind() - Bind the UDP socket to the server address.
 *
 *  socket: socket context id.
 *  address: socket address structure.
 *  address_len: length of the socket address structure.
 */
extern int 
dtp_bind(dtp_socket_t socket, const struct sockaddr *address, 
	socklen_t address_len);

/*
 *  dtp_connect() - Initialize context fields.
 *                 Register the socket to be monitored for read events.
 *
 *  socket: socket context id.
 *  address: socket address structure.
 *  address_len: length of the socket address structure.
 */
extern int
dtp_connect(dtp_socket_t socket, const struct sockaddr *address,	\
	   socklen_t address_len);

/*
 *  dtp_listen() - Create a connection queue.
 *                Register the socket to be monitored for read events.
 *
 *  socket: socket context id.
 *  backlog: maximum number of connection queues.
 */
extern int
dtp_listen(dtp_socket_t socket, int backlog);

/*
 *  dtp_accept() - Add new context id to the connection queue.
 *
 *  socket: socket context id.
 *  address: socket address structure.
 *  address_len: length of the socket address structure.
 */
extern int
dtp_accept(dtp_socket_t socket, struct sockaddr *address, \
socklen_t *address_len);

/*
 *  dtp_recv() - Read from the receive buffer.
 *
 *  socket: socket context id.
 *  buffer: data to copy from receive buffer.
 *  length: length of the data to read.
 *  flags : option argument
 */
extern ssize_t
dtp_recv(dtp_socket_t socket, void *buffer, size_t length,
	int flags);

/*
 *  dtp_read() - Read from the receive buffer.
 *
 *  socket: socket context id.
 *  buffer: data to copy from receive buffer.
 *  length: length of the data to read.
 */
extern ssize_t
dtp_read(dtp_socket_t socket, void *buffer, size_t length);

/*
 *  dtp_write() - Write to the send buffer.
 *
 *  socket: socket context id.
 *  buffer: data to copy to send buffer.
 *  length: length of the data to write.
 */
extern ssize_t
dtp_write(dtp_socket_t socket, const void *buffer, size_t length);

/*
 *  dtp_send() - Write to the send buffer.
 *
 *  socket: socket context id.
 *  buffer: data to copy to send buffer.
 *  length: length of the data to write.
 *  flags : option argument
 */
extern ssize_t
dtp_send(dtp_socket_t socket, const void *buffer, size_t length,
		 int flags);

/*
 *  dtp_sendfile() - Copies data between one fd to the another
 *
 *  out_fd: socket for output (opened for write)
 *  in_fd :  socket for input  (opened for read)
 *  offset: start reading file from offset
 *          (if it is NULL, read from the starting)
 *  count : number bytes to copy between fds
 *
 *  # currently, we only offer copy from non-DTP to DTP socket
 */
extern ssize_t
dtp_sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

/*
 *  dtp_close() - Close the corresponding context.
 *
 *  socket: socket context id.
 */
extern int
dtp_close(dtp_socket_t socket);

/*
 * dtp_select() - select function for DTP library (I/O multiplexing)
 *
 */
extern int
dtp_select(int nfds, fd_set *readfds,
	  fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

/*
 * dtp_poll() - poll function for DTP library (I/O multiplexing)
 *
 */
extern int
dtp_poll(struct pollfd *fds, unsigned nfds, int timeout);

/*
 * dtp_fcntl() - Manipulate the property of a socket descriptor
 *
 *  socket: socket context id.
 *  cmd: type of operation to be done.
 *  arg: optional arguments.
 */
extern int
dtp_fcntl(dtp_socket_t socket, int cmd, int arg);

/* command flag (dtp_fcntl) */
enum {
    DTP_F_GETFL,
    DTP_F_SETFL
};

/*
 * dtp_getsockopt() - Get options on dtp_socket_t
 *
 *  socket: socket context id.
 *  level: which level the option resides on.
 *  optname: name of socket option to get.
 *  optval: buffer where the option value to be returned.
 *  optlen: buffer where the option length to be returned.
 */

extern int
dtp_getsockopt(dtp_socket_t socket, int level, int optname,
	      void *optval, socklen_t *optlen);



/*
 * dtp_setsockopt() - Set options on dtp_socket_t
 *
 *  socket: socket context id.
 *  level: which level the option resides on.
 *  optname: name of socket option to set.
 *  optval: buffer where the option value exists.
 *  optlen: option length.
 */

extern int
dtp_setsockopt(dtp_socket_t socket, int level, int optname,
	      const void *optval, socklen_t optlen);

/*
 * dtp_getifacename() - Get the outgoing interface name
 *
 *  dest: destination address, network order.
 *  buf: where interface name will be written.
 *  len: maximum length of 'buf'.
 */
extern int
dtp_getifacename(uint32_t dest, char *buf, size_t len);

/*
 * dtp_ioctl() - ioctl function
 *
 *  socket: socket context id.
 *  request: request id.
 */

extern char* GetSDCardPath();
extern char* GetGeneratedHostID();

extern char* GetIMEI();
extern int GetIMEILength();

int
dtp_ioctl(dtp_socket_t socket, int request, ...);

/* print stats of the flow. just for debugging */
void dtp_printstat(int sock);

/* structure for dtp_setsockopt(): DTP_SO_RCVLOBUF */
struct dtp_rcvlobuf {
	uint32_t high_bound;
	uint32_t low_bound;
} __attribute__((packed));

/* option names for dtp_getsockopt and dtp_setsockopt */

enum {
    DTP_SO_ACCEPTCONN,
    DTP_SO_RCVBUF,
    DTP_SO_SNDBUF,
    DTP_SO_KEEPALIVE,
    DTP_KEEPIDLE,
    DTP_KEEPINTVL,
    DTP_KEEPCNT,
    DTP_SO_LINGER,
    DTP_SO_BLOCKSIZE,
    DTP_SO_DEADLINE,
    DTP_SO_WIFIONLY,
	DTP_UPBYTE,
	DTP_DOWNBYTE,
	DTP_MOBILETIME,
	DTP_WIFITIME,
	DTP_SO_FLOWID,
	DTP_SO_TIME,
	DTP_SO_UPCOUNT,
	DTP_SO_DOWNCOUNT,
	DTP_SO_LINK,
	DTP_NODELAY,
	DTP_SO_RCVLOBUF,
	DTP_SO_BINDTODEVICE,

    // FIX: TO BE IMPLEMENTED
    DTP_SO_RCVLOWAT,
    DTP_SO_SNDLOWAT,
    DTP_SO_RCVTIMEO,
    DTP_SO_SNDTIMEO
};

#endif /* __DTP_SOCKET_H__ */
