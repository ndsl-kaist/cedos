#ifndef __DTP_LOG_H__
#define __DTP_LOG_H__

#include <arpa/inet.h>
#include <stdbool.h>

/*
 *  dtp_getflowid() - get the flow id.
 *
 *  socket: context socket.
 */
extern uint32_t
dtp_getflowid(dtp_socket_t socket);

/*
 *  dtp_gethostid() - get the host id.
 *
 *  socket: context socket.
 */
extern u_char *
dtp_gethostid(dtp_socket_t socket);

/*
 *  dtp_getsockname() - get the socket name.
 *
 *  socket: context socket.
 *  addr: socket address.
 *  len: socket address length.
 */

/*
 * dtp_getsocklog() - Get log info on dtp_socket_t
 */
extern int
dtp_getsocklog(dtp_socket_t socket, int level, int optname,
	      void *optval, socklen_t *optlen);
#endif
