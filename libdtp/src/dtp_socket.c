#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <sched.h>
#include <assert.h>
#include <event2/event.h>
#include <time.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include "dtp_select.h"
#include "dtp_socket.h"
#include "dtp_transport.h"
#include "dtp_retrans_queue.h"
#include "dtp_mobile.h"
#include "debug.h"
#include "context.h"
#ifdef IN_MOBILE
#include "scheduler.h"
#endif

#include "dhkim_debug.h"

#define   NSEC_PER_USEC  1000L
#define   NSEC_PER_SEC  1000000000L

static bool isLibThreadRunning = FALSE;
static int inner_sock;

static bool isRSAGenerated = FALSE;
static RSA *rsa;

/*-------------------------------------------------------------------*/
static void
RunTransportThread()
{
    static int sockets[2] = {0};
    pthread_t thread;

	srand(time(NULL));

    /* create socketpair */
    if (socketpair(AF_UNIX, SOCK_DGRAM, AF_LOCAL, sockets) < 0) {
		TRACE_ERR("dtp_socket() error: socketpair error");
		EXIT(-1, return);
    }

    if (fcntl(sockets[0], F_SETFL, O_NONBLOCK) != 0) {
		TRACE_ERR("dtp_socket() error: fcntl error");
		EXIT(-1, return);
    }

    if (pthread_create(&thread, NULL, 
					   DTPLibThreadMain, (void *)&sockets[1]) < 0) {
		TRACE_ERR("dtp_socket() error: pthread_create failed\n");
		EXIT(-1, return);
    }
    inner_sock = sockets[0];   
    isLibThreadRunning = true;
}
/*-------------------------------------------------------------------*/
#ifdef IN_MOBILE

char sdpath[1000];
char hostid[1000];
char imei[1000];
int imei_len;

ssize_t
dtp_loginit(const char *path, size_t size, char* hid, char* imei_value, size_t imei_size) 
{
	strncpy(sdpath, path, size);
	//	LOGE("[[[[%s]]]]", sdpath);

	strncpy(imei, imei_value, imei_size);
	imei_len = imei_size;

	LOGE("[[[[IMEI : %s]]]]", imei);
	LOGE("[[[[IMEI_SIZE : %d]]]]", imei_size);

	u_char ori_hid[SHA1_DIGEST_LENGTH];
	DTPGenerateHostID(ori_hid);	

	hostid[0] = '\0';
	int i;
	for (i = 0; i < SHA1_DIGEST_LENGTH; i++) {
		sprintf(hostid + strlen(hostid), "%02X", ori_hid[i]);
	}

	LOGE("[[[HOSTID : %s]]]]\n", hostid);
	memcpy(hid, hostid, 40);
	
	return 40;
}

char*
GetSDCardPath() {
	return sdpath;
}
char*
GetGeneratedHostID() {
	return hostid;
}
char*
GetIMEI() {
	return imei;
}
int
GetIMEILength() {
	return imei_len;
}

#endif

/*-------------------------------------------------------------------*/

int
dtp_getsockname(dtp_socket_t socket, struct sockaddr *addr, socklen_t *len)
{
    dtp_context *ctx = NULL;

    ctx = DTPGetContextBySocket(socket);
    if (ctx)
		return getsockname(ctx->tc_sock, addr, len);
    else
		TRACE("context doesn't exist\n");
    
    return -1;
}
/*-------------------------------------------------------------------*/
int
dtp_getpeername(dtp_socket_t socket, struct sockaddr *addr, socklen_t *len)
{
    dtp_context *ctx = NULL;

    if (*len == sizeof(struct sockaddr_in)) {
		ctx = DTPGetContextBySocket(socket);
		if (ctx) {
			memcpy(addr, &ctx->tc_peerAddr, *len);
			return 0;
		}
		else {
			errno = EBADF;
			TRACE_ERR("context doesn't exist\n");
		}
    }
    else {
		errno = EINVAL;
        TRACE_ERR("wrong size len\n");
	}

    return -1;	
}
/*-------------------------------------------------------------------*/
int
dtp_socket(void) 
{
    dtp_context* ctx;
    TRACE("dtp_socket() start\n");

    /* create DTP context socket */
    ctx = DTPCreateContext();

	LOGD("dtp_socket() end\n");

    return (ctx)? ctx->tc_sock : -1;
}
/*-------------------------------------------------------------------*/
int
dtp_bind(dtp_socket_t socket, const struct sockaddr *address, 
		 socklen_t address_len) 
{
    int result;
    dtp_context* ctx;

    TRACE("dtp_bind() start\n");

    ctx = DTPGetContextBySocket(socket);
    if (ctx == NULL || !ctx->tc_isActive) {
		TRACE("invalid socket descriptor");
		errno = EBADF;
		return -1;
    }

    /* check whether socket is already bound */
    if (ctx->tc_isBound) {
		TRACE_ERR("dtp_bind() error: trying to make binding on binded socket\n");
		errno = EINVAL;
		return -1;
    }
		
    /* bind UDP socket */
    result = bind(ctx->tc_sock, (struct sockaddr*)address, address_len);
    if (result == 0)
		ctx->tc_isBound = TRUE;

    TRACE("dtp_bind() end\n");
    return result;
}

/*-------------------------------------------------------------------*/
int
dtp_connect(dtp_socket_t socket, const struct sockaddr *address, 
			socklen_t address_len) 
{
    dtp_context* ctx;
    int res;
    u_char hid[SHA1_DIGEST_LENGTH];
    dtp_event ev;
    int temp_bufLen, off = 0;
    unsigned char buf[DTP_MTU], temp_buf[RSA_LEN];

    TRACE("dtp_connect() start\n");
    if (address_len != sizeof(struct sockaddr_in)) {
		TRACE("we do not support the address format"
			  " that is not in the sockaddr_in format\n");
		return(-1);
    }

    ctx = DTPGetContextBySocket(socket);
    if (ctx == NULL) {
		TRACE("invalid socket descriptor");
		return -1;
    }

    /* create the lib thread if it's not running */
    if (!isLibThreadRunning)
		RunTransportThread();

    /* bind new physical socket to a random port number */
	/*
	  struct sockaddr_in new_address;
	  socklen_t new_addressLen = sizeof(new_address);
	  getsockname(ctx->tc_sock, (struct sockaddr*)&new_address,
	  (socklen_t*)&new_addressLen);
	  int i = 1025, res2;
	  while (1) {
	  if (++i == 65535)
	  i = 1026;
	  new_address.sin_port = htons(i);
	  res2 = bind(ctx->tc_sock, (struct sockaddr*)&new_address, new_addressLen);
	  if (res2 == 0) {
	  ctx->tc_isBound = TRUE;
	  break;
	  }
	  }
	*/

    /* set inner socket */
    ctx->tc_isock = inner_sock;

    ctx->tc_peerAddr = *(struct sockaddr_in *)address;
    ctx->tc_state = SOCK_SYN_SENT;
    ctx->tc_sendSYN = TRUE;

	/* dhkim: FIXME: just for debugging */
	/* DTP Known bug: FIXME: 
	 * If we set sequence number as random value, then calling dtp_send()
	 * by multiple thread at same time cause SIGSEGV.
	 * And, sequence number wrap around is not supported now, Nov 17, 2013
	 * Please set sequence number as 0
	 */
    ctx->tc_lastByteSent = 0;
	//  ctx->tc_lastByteSent = rand();
	ctx->tc_seqNum = 0;
//	ctx->tc_seqNum = rand();
    ctx->tc_lastByteAcked = ctx->tc_seqNum;
    ctx->tc_lastByteWritten = ctx->tc_seqNum;
    ctx->tc_lastByteSent = ctx->tc_seqNum;

    DTPGenerateHostID(hid);
    while((ctx->tc_flowID = DTPGenerateFlowID(hid)) == 0xFFFFFFFF)
		usleep(1000);

    /* initialize RSA key pair */
    if (!isRSAGenerated) {
		rsa = DTPInitializeRSAKey();
		isRSAGenerated = TRUE;
    }

    /* public key (n) */
    temp_bufLen = BN_bn2bin(rsa->n, temp_buf);
    memcpy(buf + off, &temp_bufLen, 4);
    off += 4;
    memcpy(buf + off, temp_buf, temp_bufLen);
    off += temp_bufLen;
    /* public key (e) */
    temp_bufLen = BN_bn2bin(rsa->e, temp_buf);
    memcpy(buf + off, &temp_bufLen, 4);
    off += 4;
    memcpy(buf + off, temp_buf, temp_bufLen);
    off += temp_bufLen;

    /* stored RSA public key into send buffer */
    ctx->tc_keyLen = off;
    ctx->tc_lastByteWritten += off;
    memcpy(ctx->tc_writeBuf, buf, off);

	DTPWriteAvail(socket, ctx);

    /* register this socket to be monitored for read events */
    if (!ctx->tc_beingMonitored) {
		ctx->tc_beingMonitored = true;
		ev.te_fd = ctx->tc_sock;
		ev.te_command = DTP_ADD_READ_EVENT;
		DTPSendEventToLibThread(inner_sock, &ev);
    }

    TRACE("dtp_connect() end\n");
	LOGD("dtp_connect() end\n");
    res = 0; /* FIX this! */
    return (res);
}
/*-------------------------------------------------------------------*/
int
dtp_listen(dtp_socket_t socket, int backlog) 
{
    dtp_context* ctx;
	int* ptr;

    TRACE("dtp_listen() start\n");

    ctx = DTPGetContextBySocket(socket);
    if (ctx == NULL) {
		TRACE("invalid socket descriptor");
		errno = ENOTSOCK;
		return -1;
    }

    /* check whether socket is already bound */  
    if (ctx->tc_isBound == FALSE) {
		TRACE_ERR("dtp_listen() error: trying to listen to unbinded socket\n");
		errno = EINVAL;
		return -1;
    }
	
    /* check socket state */
    if (ctx->tc_state != SOCK_CLOSED) {
		if (ctx->tc_state == SOCK_LISTEN && ctx->tc_qidx == 0) {
			ptr = (int *)realloc(ctx->tc_listenQ, sizeof(int) * backlog);
			if (ptr == NULL) {
				FREE_MEM(ctx->tc_listenQ);
				TRACE("failed to reallocate backlog queue\n");
				errno = ENOMEM;
				return -1;
			}
			ctx->tc_listenQ = ptr;
			ctx->tc_backlog = backlog;
			return 0;
		}
		else {
			TRACE_ERR("dtp_listen() error: not a valid socket to listen\n");
			errno = EINVAL;
			return -1;
		}
    }
	
    /* set socket state */
    ctx->tc_state = SOCK_LISTEN;

    /* create the connection queue of size "backlog" */
    ctx->tc_listenQ = (int *)malloc(sizeof(int) * backlog);
    if (ctx->tc_listenQ == NULL) {
		TRACE("failed in creating the listen Q\n");
		errno = ENOMEM;		
		return -1;
    }
    ctx->tc_qidx = 0;
    ctx->tc_backlog = backlog;
	
    TRACE("dtp_listen() end\n");
    return 0;
}

/*-------------------------------------------------------------------*/
int
dtp_accept(dtp_socket_t sock, struct sockaddr *address, 
		   socklen_t *address_len) 
{
    dtp_context* ctx;
    int new_fd;
    int res;
    dtp_event ev;

    TRACE("dtp_accept start\n");

    ctx = DTPGetContextBySocket(sock);
    if (ctx == NULL || !ctx->tc_isActive) {
		TRACE_ERR("invalid socket descriptor");
		errno = EBADF;
		return -1;
    }

    /* check socket state */
    if (ctx->tc_state != SOCK_LISTEN) {
		TRACE_ERR("not a valid socket to accept\n");
		errno = EINVAL;
		return -1;
    }

    /* create the lib thread if it's not running */
    if (!isLibThreadRunning)
		RunTransportThread();
    
    /* set inner socket */
    ctx->tc_isock = inner_sock;

    /* register this socket to be monitored for read events */
    if (!ctx->tc_beingMonitored) {
		ctx->tc_beingMonitored = true;
		ev.te_fd = ctx->tc_sock;
		ev.te_command = DTP_ADD_LISTEN_EVENT;
		DTPSendEventToLibThread(inner_sock, &ev);
    }

    /* acquire a lock to access the shared variables */
    if (pthread_mutex_lock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		return -1;
    }

    while (ctx->tc_qidx == 0) {
		/* nonblocking socket */	
		if (ctx->tc_fstatus & O_NONBLOCK) {
			if (pthread_mutex_unlock(&ctx->tc_readBufLock)) {
				TRACE_ERR("pthread_mutex_unlock() failed");
				return -1;
			}

			errno = EAGAIN;
			return -1;
		}

		ctx->tc_isReadBlocked = TRUE;

		if (pthread_cond_wait(&ctx->tc_readBufCond, 
							  &ctx->tc_readBufLock)) {
			TRACE_ERR("pthread_cond_wait() failed");
			return -1;
		}

		ctx->tc_isReadBlocked = FALSE;
		if (ctx->tc_isTransportWaiting) {
			if (pthread_cond_signal(&ctx->tc_readBufCond)) {
				TRACE("pthread_cond_signal failed\n");
				return -1;
			}
		}
    }

    new_fd = ctx->tc_listenQ[0];
    ctx->tc_qidx--;

    // [44] if there is no more waiting connection, clear!
    if (ctx->tc_qidx == 0) {
		/* dhkim: XXX: Is this weird or not??
		   if (ctx->tc_state & (SOCK_CLOSE_WAIT | SOCK_LAST_ACK))
		   DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
		   "fid=%08X: Trying to DTPSelectEventClr() in SOCK_CLOSE_WAIT state",
		   ctx->tc_flowID);
		*/
		DTPSelectEventClr(sock, DTP_FD_READ);
    }
    
    if (ctx->tc_qidx)
		memmove(&ctx->tc_listenQ[0], &ctx->tc_listenQ[1], 
				sizeof(ctx->tc_listenQ[0]) * ctx->tc_qidx);
    
    if (address) {
		if (!address_len || (*address_len) < 0) {
			errno = EINVAL;
			return -1;
		}
		res = dtp_getpeername(new_fd, address, address_len);
		if (res == -1) {
			TRACE_ERR("dtp_getpeername() failed.");
			return -1;			
		}		
    }

    if (pthread_mutex_unlock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		return -1;
    }

    TRACE("dtp_accept end\n");

    return new_fd;
}

/*-------------------------------------------------------------------*/
ssize_t
dtp_recv(dtp_socket_t socket, void *buf, size_t length, int flags) 
{
    int len, moveLen;
    dtp_context* ctx;
	u_char *p;

    TRACE("dtp_recv() start\n");

    ctx = DTPGetContextBySocket(socket);
    if (ctx == NULL || !ctx->tc_isActive) {
		TRACE("invalid socket descriptor");
		errno = EBADF;
		return -1;
    }

    /* connection timed out */
    if (ctx->tc_connTimedOut) {
		errno = ETIMEDOUT;
		return -1;
    }
    /* connection reset */
    if (ctx->tc_connReset) {
		errno = ECONNRESET;
		return -1;
    }
    /* if length is zero, nothing to read */
    if (length == 0)
		return 0;

    /* acquire a lock to access the shared variables */
    if (pthread_mutex_lock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		return -1;
    }

    len = __min(length, GetReadBufDataSize(ctx));

    while (len == 0) {
		/* connection closed */
		if ((ctx->tc_state == SOCK_CLOSE_WAIT
			 || ctx->tc_state == SOCK_LAST_ACK
			 || ctx->tc_state == SOCK_CLOSED
			 || ctx->tc_state == SOCK_FIN_WAIT_1
			 || ctx->tc_state == SOCK_FIN_WAIT_2
			 || ctx->tc_state == SOCK_TIME_WAIT))
			break;
	
		/* nonblocking socket */	
		if (ctx->tc_fstatus & O_NONBLOCK) {
			errno = EAGAIN;
			len = -1;
			break;
		}

		/* nothing to read, so let's block here */
		ctx->tc_isReadBlocked = TRUE;
		if (pthread_cond_wait(&ctx->tc_readBufCond, 
							  &ctx->tc_readBufLock)) {
			TRACE_ERR("pthread_cond_wait() failed");
			return -1;
		}
		ctx->tc_isReadBlocked = FALSE;

		/* connection timed out */
		if (ctx->tc_connTimedOut) {
			errno = ETIMEDOUT;
			len = -1;
			break;
		}
		/* connection reset */
		if (ctx->tc_connReset) {
			errno = ECONNRESET;
			len = -1;
			break;
		}

		len = __min(length, GetReadBufDataSize(ctx));

		if (ctx->tc_isTransportWaiting) {
			if (pthread_cond_signal(&ctx->tc_readBufCond)) {
				TRACE("pthread_cond_signal failed\n");
				return -1;
			}
		}

		/* check any mishavior happened */
		ASSERT(!(len > 0 && ctx->tc_state == SOCK_CLOSE_WAIT), );
    }

    if (len > 0) {
		if (ctx->tc_lastByteRead != ctx->tc_readBufHead->seq || len > ctx->tc_readBufHead->len) {
			LOGD("[sock %d, A][mismatch!] lBRead : %d / nBExpected : %d / len : %d / headBufSeq : %d / headBuflen : %d",
				 ctx->tc_sock, ctx->tc_lastByteRead, ctx->tc_nextByteExpected, len, ctx->tc_readBufHead->seq, ctx->tc_readBufHead->len);
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
					 "[sock %d, A][mismatch!] "
					 "lBRead : %d / nBExpected : %d / len : %d / "
					 "headBufSeq : %d / headBuflen : %d",
					 ctx->tc_sock, ctx->tc_lastByteRead, 
					 ctx->tc_nextByteExpected, len, 
					 ctx->tc_readBufHead->seq, ctx->tc_readBufHead->len);
			EXIT(-1, );
		}

		p = ctx->tc_readBuf;
		memcpy(buf, p, len);

		if (!(flags & MSG_PEEK)) {
			ctx->tc_lastByteRead += len;

			/* if len > 0, it should not be NULL */
			ASSERT(ctx->tc_readBufHead,
				   pthread_mutex_unlock(&ctx->tc_readBufLock);
				   return 0;);

			/* slide readBufHead */
			ctx->tc_readBufHead->seq += len;
			ctx->tc_readBufHead->len -= len;
			/* free readBufHead if it is drained */
			if (ctx->tc_readBufHead->len == 0) {
				buffer* b;
				b = ctx->tc_readBufHead;
				ctx->tc_readBufHead = ctx->tc_readBufHead->next_buf;
				AddToFreeBufferList(b);

				if (GetReadBufDataSize(ctx) > 0 && ctx->tc_lastByteRead != ctx->tc_readBufHead->seq) {
					LOGD("[sock %d, C][mismatch!] lBRead : %d / nBExpected : %d / readBufSeq : %d / headBuflen : %d",
						 ctx->tc_sock, ctx->tc_lastByteRead, ctx->tc_nextByteExpected, ctx->tc_readBufHead->seq, ctx->tc_readBufHead->len);	
					DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
							 "[sock %d, C][mismatch!] lBRead : %d / "
							 "nBExpected : %d / readBufSeq : %d / "
							 "headBuflen : %d",
							 ctx->tc_sock, ctx->tc_lastByteRead, 
							 ctx->tc_nextByteExpected, 
							 ctx->tc_readBufHead->seq, 
							 ctx->tc_readBufHead->len);
					EXIT(-1, );
				}				
			}

			moveLen = ctx->tc_lastByteRcvd - ctx->tc_lastByteRead;
			if (moveLen > 0) 
				memmove(p, p + len, moveLen);

			// [5] if there is no more data to read, set rd off.
			if (GetReadBufDataSize(ctx) == 0) {
				if (ctx->tc_isFINRcvd)
					DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ);
				else {
#if 0
					if (ctx->tc_state & (SOCK_CLOSE_WAIT | SOCK_LAST_ACK))
						DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
								 "fid=%08X: Trying to DTPSelectEventClr() in SOCK_CLOSE_WAIT state",
								 ctx->tc_flowID);
#endif
					/* do not clear select event if connection is closing */
					if (!(ctx->tc_state == SOCK_CLOSE_WAIT
						  || ctx->tc_state == SOCK_LAST_ACK
						  || ctx->tc_state == SOCK_FIN_WAIT_1
						  || ctx->tc_state == SOCK_FIN_WAIT_2
						  || ctx->tc_state == SOCK_TIME_WAIT))
						DTPSelectEventClr(ctx->tc_sock, DTP_FD_READ);
				}
			}

			if (ctx->tc_isMyWindowZero) {
				if ((GetAvailReadBufSize(ctx) >> ctx->tc_recvWindowScale) > 0) {
					ctx->tc_isMyWindowZero = FALSE;
					DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
					DTPSendACKPacket(ctx);
				}
			}

			/* FIX : it's only for DOWNLINK deadline */
			ctx->tc_blockRemain -= len;

		}
    }
	
    if (pthread_mutex_unlock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		return -1;
    }
	
	if (len > 0) {
		if (ctx->tc_sock == 100) {
			DHK_FLOG(DHK_DEBUG & DMISMATCH, DHK_F_BASE"error.txt",
					 "dtp_recv() returns [sock %d] "
					 "lBRead : %d / nBExpected : %d / len : %d / "
					 "headBufSeq : %d / headBuflen : %d",
					 ctx->tc_sock, ctx->tc_lastByteRead, 
					 ctx->tc_nextByteExpected, len, 
					 ctx->tc_readBufHead ? ctx->tc_readBufHead->seq : -1,
					 ctx->tc_readBufHead ? ctx->tc_readBufHead->len : -1);
		}
	}
    TRACE("dtp_recv() end\n");
    return len;
}
/*-------------------------------------------------------------------*/
ssize_t
dtp_read(dtp_socket_t socket, void *buf, size_t length) 
{
    return dtp_recv(socket, buf, length, 0);
}
/*-------------------------------------------------------------------*/
ssize_t
dtp_sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
    int avail;
    int len;
    dtp_context* ctx;

	if (!(dtp_isdtpsocket(out_fd) && !dtp_isdtpsocket(in_fd))) {
		TRACE_ERR("we only support non-DTP to DTP sendfile now");
		errno = EBADF;
		return -1;
	}
	
	/*
	 * NOTE: Hereby, we can suppose that out_fd is DTP socket,
	 *       and in_fd is non-DTP socket. (Ad-hoc implementation)
	 */

    ctx = DTPGetContextBySocket(out_fd);
    if (ctx == NULL || !ctx->tc_isActive) {
		TRACE("invalid socket descriptor");
		errno = EBADF;
		return -1;
    }

    /* if count is 0, nothing to write */
    if (count == 0)
		return 0;

	/* connection closed */
	if ((ctx->tc_state == SOCK_CLOSE_WAIT
		 || ctx->tc_state == SOCK_LAST_ACK
		 || ctx->tc_state == SOCK_CLOSED
		 || ctx->tc_state == SOCK_FIN_WAIT_1
		 || ctx->tc_state == SOCK_FIN_WAIT_2
		 || ctx->tc_state == SOCK_TIME_WAIT)) {
		errno = EPIPE;
		return -1;
	}

	/* connection reset */
	if (ctx->tc_connReset) {
		errno = ECONNRESET;
		return -1;
	}

    /* acquire a lock to access the shared variables */
    if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		return -1;
    }

    avail = GetAvailWriteBufSize(ctx);
    len = __min(count, avail);

    while (len == 0) {

		/* connection closed */
		if ((ctx->tc_state == SOCK_CLOSE_WAIT
			 || ctx->tc_state == SOCK_LAST_ACK
			 || ctx->tc_state == SOCK_CLOSED
			 || ctx->tc_state == SOCK_FIN_WAIT_1
			 || ctx->tc_state == SOCK_FIN_WAIT_2
			 || ctx->tc_state == SOCK_TIME_WAIT)) {
			errno = EPIPE;
			len = -1;
			break;
		}

		/* connection timed out */
		if (ctx->tc_connTimedOut) {
			errno = ETIMEDOUT;
			len = -1;
			break;
		}

		/* connection reset */
		if (ctx->tc_connReset) {
			errno = ECONNRESET;
			len = -1;
			break;
		}

		/* nonblocking socket */
		if (ctx->tc_fstatus & O_NONBLOCK) {
			errno = EAGAIN;
			len = -1;
			break;
		}

		/* no available write buffer, so wait until we find available buffer */
		ctx->tc_isWriteBlocked = TRUE;
		if (pthread_cond_wait(&ctx->tc_writeBufCond, 
							  &ctx->tc_writeBufLock)) {
			TRACE_ERR("pthread_cond_wait() failed");
			return -1;
		}
		ctx->tc_isWriteBlocked = FALSE;
		avail = GetAvailWriteBufSize(ctx);
		len = __min(count, avail);
    }

    if (len > 0) {
		//memcpy(ctx->tc_writeBuf + GetWriteBufOff(ctx), buf, len);
		
		if (offset != NULL)
			if (lseek(in_fd, *offset, SEEK_SET) == -1) {
				TRACE_ERR("lseek() error\n");
				len = -1;
			}
		
		if (len != -1)
			len = read(in_fd, ctx->tc_writeBuf + GetWriteBufOff(ctx), len);

		if (lseek(in_fd, 0, SEEK_END) == -1) {
			TRACE_ERR("lseek() error\n");
			len = -1;
		}		

		if (len > 0) {
			ctx->tc_lastByteWritten += len;

			// [4] if buffer became full, clear write bit to zero
			if (GetAvailWriteBufSize(ctx) == 0) {
#if 0
				if (ctx->tc_state & (SOCK_CLOSE_WAIT | SOCK_LAST_ACK))
					DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
							 "fid=%08X: Trying to DTPSelectEventClr() in %s state",
							 ctx->tc_flowID,
							 ctx->tc_state == SOCK_CLOSE_WAIT
							 ? "SOCK_CLOSE_WAIT" : "SOCK_LAST_ACK");
#endif
				/* do not clear select event if connection is closing */
				if (!(ctx->tc_state == SOCK_CLOSE_WAIT
					  || ctx->tc_state == SOCK_LAST_ACK
					  || ctx->tc_state == SOCK_FIN_WAIT_1
					  || ctx->tc_state == SOCK_FIN_WAIT_2
					  || ctx->tc_state == SOCK_TIME_WAIT))
					DTPSelectEventClr(ctx->tc_sock, DTP_FD_WRITE);
			}

			/* try sending the data to the peer */
			DTPWriteAvail(out_fd, ctx);
		}
    }

    if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		return -1;
    }

    TRACE("dtp_sendfile() end\n");
    return len;

}

/*-------------------------------------------------------------------*/
ssize_t
dtp_send(dtp_socket_t socket, const void *buf, size_t length, int flags) 
{

    int avail;
    int len;
    dtp_context* ctx;

    TRACE("dtp_send() start sock=%d length=%u\n", socket, (uint32_t)length);

    ctx = DTPGetContextBySocket(socket);
    if (ctx == NULL || !ctx->tc_isActive) {
		TRACE("invalid socket descriptor");
		errno = EBADF;
		return -1;
    }

    /* if length is 0, nothing to write */
    if (length == 0)
		return 0;

	/* connection closed */
	if ((ctx->tc_state == SOCK_CLOSE_WAIT
		 || ctx->tc_state == SOCK_LAST_ACK
		 || ctx->tc_state == SOCK_CLOSED
		 || ctx->tc_state == SOCK_FIN_WAIT_1
		 || ctx->tc_state == SOCK_FIN_WAIT_2
		 || ctx->tc_state == SOCK_TIME_WAIT)) {
		errno = EPIPE;
		return -1;
	}

	/* connection reset */
	if (ctx->tc_connReset) {
		errno = ECONNRESET;
		return -1;
	}

    /* acquire a lock to access the shared variables */
    if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		return -1;
    }

    avail = GetAvailWriteBufSize(ctx);
    len = __min(length, avail);

    while (len == 0) {

		/* connection closed */
		if ((ctx->tc_state == SOCK_CLOSE_WAIT
			 || ctx->tc_state == SOCK_LAST_ACK
			 || ctx->tc_state == SOCK_CLOSED
			 || ctx->tc_state == SOCK_FIN_WAIT_1
			 || ctx->tc_state == SOCK_FIN_WAIT_2
			 || ctx->tc_state == SOCK_TIME_WAIT)) {
			errno = EPIPE;
			len = -1;
			break;
		}

		/* connection timed out */
		if (ctx->tc_connTimedOut) {
			errno = ETIMEDOUT;
			len = -1;
			break;
		}

		/* connection reset */
		if (ctx->tc_connReset) {
			errno = ECONNRESET;
			len = -1;
			break;
		}

		/* nonblocking socket */
		if (ctx->tc_fstatus & O_NONBLOCK) {
			errno = EAGAIN;
			len = -1;
			break;
		}

		/* no available write buffer, so wait until we find available buffer */
		ctx->tc_isWriteBlocked = TRUE;
		if (pthread_cond_wait(&ctx->tc_writeBufCond, 
							  &ctx->tc_writeBufLock)) {
			TRACE_ERR("pthread_cond_wait() failed");
			return -1;
		}
		ctx->tc_isWriteBlocked = FALSE;
		avail = GetAvailWriteBufSize(ctx);
		len = __min(length, avail);
    }

    if (len > 0) {
		memcpy(ctx->tc_writeBuf + GetWriteBufOff(ctx), buf, len);
		ctx->tc_lastByteWritten += len;

		// [4] if buffer became full, clear write bit to zero
		if (GetAvailWriteBufSize(ctx) == 0) {
#if 0
			if (ctx->tc_state & (SOCK_CLOSE_WAIT | SOCK_LAST_ACK))
				DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
						 "fid=%08X: Trying to DTPSelectEventClr() in %s state",
						 ctx->tc_flowID,
						 ctx->tc_state == SOCK_CLOSE_WAIT
						 ? "SOCK_CLOSE_WAIT" : "SOCK_LAST_ACK");
#endif
			/* do not clear select event if connection is closing */
			if (!(ctx->tc_state == SOCK_CLOSE_WAIT
				  || ctx->tc_state == SOCK_LAST_ACK
				  || ctx->tc_state == SOCK_FIN_WAIT_1
				  || ctx->tc_state == SOCK_FIN_WAIT_2
				  || ctx->tc_state == SOCK_TIME_WAIT))
				DTPSelectEventClr(ctx->tc_sock, DTP_FD_WRITE);
		}

		/* try sending the data to the peer */
		DTPWriteAvail(socket, ctx);
    }

    if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		return -1;
    }

    TRACE("dtp_send() end\n");
    return len;
}

/*-------------------------------------------------------------------*/
ssize_t
dtp_write(dtp_socket_t socket, const void *buf, size_t length) 
{
	return dtp_send(socket, buf, length, 0);
}

#ifdef EXPERIMENTAL
/*-------------------------------------------------------------------*/
ssize_t
dtp_writev(dtp_socket_t socket, const struct iovec *iov, int iovcnt) 
{

    int avail;
    int len;
    dtp_context* ctx;

    TRACE("dtp_send() start sock=%d length=%u\n", socket, (uint32_t)length);

    ctx = DTPGetContextBySocket(socket);
    if (ctx == NULL || !ctx->tc_isActive) {
		TRACE("invalid socket descriptor");
		errno = EBADF;
		return -1;
    }

    /* if length is 0, nothing to write */
    if (length == 0)
		return 0;

    /* acquire a lock to access the shared variables */
    if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		return -1;
    }

    avail = GetAvailWriteBufSize(ctx);
    len = __min(length, avail);

    while (len == 0) {

		/* connection closed */
		if ((ctx->tc_state == SOCK_CLOSE_WAIT
			 || ctx->tc_state == SOCK_LAST_ACK
			 || ctx->tc_state == SOCK_CLOSED)) {
			errno = EPIPE;
			len = -1;
			break;
		}

		/* connection timed out */
		if (ctx->tc_connTimedOut) {
			errno = ETIMEDOUT;
			len = -1;
			break;
		}

		/* connection reset */
		if (ctx->tc_connReset) {
			errno = ECONNRESET;
			len = -1;
			break;
		}

		/* nonblocking socket */
		if (ctx->tc_fstatus & O_NONBLOCK) {
			errno = EAGAIN;
			len = -1;
			break;
		}

		/* no available write buffer, so wait until we find available buffer */
		ctx->tc_isWriteBlocked = TRUE;
		if (pthread_cond_wait(&ctx->tc_writeBufCond, 
							  &ctx->tc_writeBufLock)) {
			TRACE_ERR("pthread_cond_wait() failed");
			return -1;
		}
		ctx->tc_isWriteBlocked = FALSE;
		avail = GetAvailWriteBufSize(ctx);
		len = __min(length, avail);
    }

    if (len > 0) {
		memcpy(ctx->tc_writeBuf + GetWriteBufOff(ctx), buf, len);
		ctx->tc_lastByteWritten += len;

		// [4] if buffer became full, clear write bit to zero
		if (GetAvailWriteBufSize(ctx) == 0) {
			if ((ctx->tc_state == SOCK_CLOSE_WAIT
				 || ctx->tc_state == SOCK_LAST_ACK
				 || ctx->tc_state == SOCK_FIN_WAIT_1
				 || ctx->tc_state == SOCK_FIN_WAIT_2
				 || ctx->tc_state == SOCK_TIME_WAIT))
				DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
						 "fid=%08X: Trying to DTPSelectEventClr() in SOCK_CLOSE_WAIT state",
						 ctx->tc_flowID);
			DTPSelectEventClr(ctx->tc_sock, DTP_FD_WRITE);
		}

		/* try sending the data to the peer */
		DTPWriteAvail(socket, ctx);
    }

    if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		return -1;
    }

    TRACE("dtp_send() end\n");
    return len;
}
#endif
/*-------------------------------------------------------------------*/
int
dtp_close(dtp_socket_t socket) 
{
    TRACE("dtp_close() start\n");

    dtp_context *ctx;
    ctx = DTPGetContextBySocket(socket);

	DHK_FLOG(DHK_DEBUG & DTEMP, DHK_F_BASE"/dtp_close.txt",
			 "DTP%08X", ctx->tc_flowID);

    if (ctx == NULL || !ctx->tc_isActive) {
		TRACE("invalid socket descriptor");
		errno = EBADF;
		DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
				 "Invalid socket descriptor. socket %d", socket);
		return -1;
    }

    ctx->tc_closeCalled = TRUE;

#ifdef IN_MOBILE
	LOGD("START!!!");
		
	struct timeval tv;
	double now;
	if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, );
	}
	now = tv.tv_sec + (tv.tv_usec / 1e6);

	char filename[500];
	char today[50];
	strftime(today, 50, "%Y%m%d", localtime(&tv.tv_sec));
	sprintf(filename, "%s%s_D_%s.txt", sdpath, today, hostid);
	int networkTypeLast = GetNetworkTypeLast();
	uint32_t ipaddrLast = GetIpaddrLast();
	
	FILE* fd;
	fd = fopen(filename, "a+");
	if (fd != NULL) {
		fprintf(fd, "%u %d %d.%d.%d.%d %s %d %.3f %.3f %d\n",
				ctx->tc_flowID,
				networkTypeLast,
				(ipaddrLast >> 24)&0xff,(ipaddrLast >> 16)&0xff,
				(ipaddrLast >> 8)&0xff, ipaddrLast&0xff,
				//				inet_ntoa(ctx->tc_sockAddr.sin_addr),    /* my ip addr */
				inet_ntoa(ctx->tc_peerAddr.sin_addr),    /* peer ip addr */
				ctx->tc_blockRemainLast - ctx->tc_blockRemain,
				(ctx->tc_networkTimeLast)? ctx->tc_networkTimeLast : now,
				(ctx->tc_networkTimeLast)? now - ctx->tc_networkTimeLast : 0,
				ctx->tc_schedAllowTime);
		fflush(fd);
		fclose(fd);	
	}

	LOGP("[%u %d %d.%d.%d.%d %s %d %.3f %.3f %d]",
		 ctx->tc_flowID,
		 networkTypeLast,
		 (ipaddrLast >> 24)&0xff,(ipaddrLast >> 16)&0xff,
		 (ipaddrLast >> 8)&0xff, ipaddrLast&0xff,
		 //		 inet_ntoa(ctx->tc_sockAddr.sin_addr),    /* my ip addr */
		 inet_ntoa(ctx->tc_peerAddr.sin_addr),    /* peer ip addr */
		 ctx->tc_blockRemainLast - ctx->tc_blockRemain,
		 (ctx->tc_networkTimeLast)? ctx->tc_networkTimeLast : now,
		 (ctx->tc_networkTimeLast)? now - ctx->tc_networkTimeLast : 0,
		 ctx->tc_schedAllowTime);

	/* to handle conn reset right here (YGMOON)
	   if (ctx->tc_connReset == TRUE) {
	   DTPCloseContext(ctx);
	   return 0;
	   }
	*/

#endif /* IN_MOBILE */

    if (ctx->tc_state != SOCK_CLOSED && ctx->tc_connReset == FALSE) {
		/* acquire a lock to access the shared variables */
		if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
			TRACE_ERR("pthread_mutex_lock() failed");
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
					 "pthread_mutex_lock() failed. socket %d", socket);
			return -1;
		}
	
		/* send FIN packet after all data packets are sent */
		DTPWriteAvail(ctx->tc_sock, ctx);

		/* linger option is enabled (graceful shutdown) */
		if (ctx->tc_useLinger) {
			TRACE("wait until teardown process is done\n");
			ctx->tc_waitACK = TRUE;
			if (pthread_cond_wait(&ctx->tc_closeCond, 
								  &ctx->tc_writeBufLock)) {
				TRACE_ERR("pthread_cond_wait() failed");
				DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
						 "pthread_cond_wait() failed. socket %d", socket);
				return -1;
			}
			ctx->tc_waitACK = FALSE;
		}

		TRACE("teardown process completed\n");
	
		/* release the lock */
		if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
			int errno_b = errno;
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
					 "pthread_mutex_unlock() failed. %d:%s socket %d",
					 errno_b, strerror(errno_b), socket);
			TRACE_ERR("pthread_mutex_unlock() failed");
			return -1;
		}
    }

	/* FIXME: make sure nothing is left */
	//	DTPWriteAvail(ctx->tc_sock, ctx);
    
    /* final close! */
    if (ctx->tc_useLinger) {
        DTPCloseContext(ctx);
	}

    TRACE("dtp_close() end\n");
    return 0;
}
/*-------------------------------------------------------------------*/
bool
dtp_isdtpsocket(dtp_socket_t socket)
{
    return DTPIsDTPSocket(socket);
}
/*-------------------------------------------------------------------*/
int
dtp_select(int nfds, fd_set *readfds,
		   fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
    dtp_event ev;
    int ret_val = 0;
    dtp_context* ctx;
    int i, res;
    bool dtpHasAnyEvent = false;

    int maxidx = (nfds + __NFDBITS - 1) / __NFDBITS;
    int count_tcp, count_dtp;

	struct timeval timeout_temp;

    /* fd_sets for demultiplexing tcp and dtp sockets */
    fd_set_s readfds_tcp, writefds_tcp;
    fd_set_s readfds_dtp, writefds_dtp;
    fd_set_s readfds_dtp_temp, writefds_dtp_temp;
    fd_set_s readfds_tcp_temp, writefds_tcp_temp;
    fd_set_s *dtp_filter = DTPGetFdSet();

    TRACE("dtp_select() start\n");

    if (exceptfds != NULL) {
		TRACE("exceptfds is not supported yet.");
		return -1;
    }
    if (nfds < 0) {
		errno = EINVAL;
		return -1;
    }

    /* create the lib thread if it's not running */
    if (!isLibThreadRunning)
		RunTransportThread();

    for (i = 0; i < nfds; i++) {
		if (dtp_isdtpsocket(i) && FD_ISSET_S(i, readfds)) {
			ctx = DTPGetContextBySocket(i);
			ASSERT(ctx, return -1);
			/* set inner socket */
			ctx->tc_isock = inner_sock;
			if (ctx->tc_sock == i && !ctx->tc_beingMonitored) {
				ctx->tc_beingMonitored = true;
				ev.te_fd = ctx->tc_sock;
				ev.te_command = DTP_ADD_LISTEN_EVENT;
				DTPSendEventToLibThread(inner_sock, &ev);
			}
		}
    }

    if (readfds) {
		FD_ZERO_S(&readfds_dtp);
		FD_ZERO_S(&readfds_tcp);

		for (i = 0; i < maxidx; i++) {
			(__FDS_BITS(&readfds_dtp))[i] = (__FDS_BITS(readfds))[i]
				& (__FDS_BITS(dtp_filter))[i];
			((__FDS_BITS(&readfds_tcp))[i]) = (__FDS_BITS(readfds))[i]
				& ~((__FDS_BITS(dtp_filter))[i]);
		}
		readfds_dtp_temp = readfds_dtp;
		readfds_tcp_temp = readfds_tcp;
    }

    if (writefds) {
		FD_ZERO_S(&writefds_dtp);
		FD_ZERO_S(&writefds_tcp);

		for (i = 0; i < maxidx; i++) {
			(__FDS_BITS(&writefds_dtp))[i] = (__FDS_BITS(writefds))[i]
				& (__FDS_BITS(dtp_filter))[i];
			((__FDS_BITS(&writefds_tcp))[i]) = (__FDS_BITS(writefds))[i]
				& ~((__FDS_BITS(dtp_filter))[i]);
		}
		writefds_dtp_temp = writefds_dtp;
		writefds_tcp_temp = writefds_tcp;
    }
        
    count_dtp = DTPSelectCheckAnyPendingEvent(nfds, (readfds) ? &readfds_dtp : NULL,
											  (writefds) ? &writefds_dtp : NULL);
    if (count_dtp > 0) {
		struct timeval zero;
		zero.tv_sec = 0;
		zero.tv_usec = 0;
		count_tcp = select(nfds, (readfds) ? (fd_set *)&readfds_tcp : NULL,
						   (writefds) ? (fd_set *) &writefds_tcp : NULL, NULL, &zero);
		dtpHasAnyEvent = true;
    }
    else {
		if (readfds)
			readfds_dtp = readfds_dtp_temp;
		if (writefds)
			writefds_dtp = writefds_dtp_temp;

		while (1) {
			if (timeout != NULL) {
				timeout_temp.tv_sec = timeout->tv_sec;
				timeout_temp.tv_usec = timeout->tv_usec;
			}

			if (!readfds)
				FD_ZERO_S(&readfds_tcp);
			/* register inner_sock to readfds_tcp set */
			FD_SET_S(inner_sock, &readfds_tcp); 
			/* if inner_sock is the largest one,
			   set nfds = inner_sock + 1 to monitor 0 ~ inner_sock */
			if (inner_sock >= nfds)
				nfds = inner_sock + 1;

			/* deliver the fd_sets that i'm interested in */
			DTPSelectInit(nfds, &readfds_dtp, &writefds_dtp);

			/* wait in the select */
			count_tcp = select(nfds, (readfds) ? (fd_set *)&readfds_tcp : NULL,
							   (writefds) ? (fd_set *)&writefds_tcp : NULL, NULL,
							   (timeout) ? &timeout_temp : NULL);

			if (count_tcp == 0 || count_tcp == -1) {
				/* flush inner_sock buffer */
				int errno_temp;
				errno_temp = errno;
				while ((res = read(inner_sock, &ev, sizeof(ev))) > 0);
				errno = errno_temp;

				/* say that i'm not waiting in select anymore. */
				DTPSelectClear();

				if (readfds)
					FD_ZERO_S(readfds);
				if (writefds)
					FD_ZERO_S(writefds);
				return count_tcp;
			}
    
			/* count_tcp > 0 */
			/* if dtp socket has any event */
			if (FD_ISSET_S(inner_sock, &readfds_tcp)) {
				if ((res = read(inner_sock, &ev, sizeof(ev))) != sizeof(ev)) {
					TRACE("read() ret=%d errno=%d failed\n", res, errno);
					/* say that i'm not waiting in select anymore. */
					DTPSelectClear();
					return -1;
				}
				count_tcp = count_tcp - 1; /* exclude the inner_sock count */
				FD_CLR_S(inner_sock, &readfds_tcp);
				count_dtp = DTPSelectCheckAnyPendingEvent(nfds,
														  (readfds)?&readfds_dtp : NULL,
														  (writefds)?&writefds_dtp : NULL);

				/* there might be no pending event
				   (i.e. write event was set, but no writefd set)
				   --> in this case going back to loop */
				if (count_dtp > 0)
					dtpHasAnyEvent = true;
			}

			if (count_tcp > 0 || count_dtp > 0) {
				DTPSelectClear();
				break;
			}
			else {
				if (readfds) {
					readfds_dtp = readfds_dtp_temp;
					readfds_tcp = readfds_tcp_temp;
				}
				if (writefds) {
					writefds_dtp = writefds_dtp_temp;
					writefds_tcp = writefds_tcp_temp;
				}
			}

		}

    }

    if (dtpHasAnyEvent) {
		for (i = 0; i < maxidx; i++) {
			if (readfds)
				(__FDS_BITS(readfds))[i] = (__FDS_BITS(&readfds_dtp))[i]
					| (__FDS_BITS(&readfds_tcp))[i];
			if (writefds)
				(__FDS_BITS(writefds))[i] = (__FDS_BITS(&writefds_dtp))[i]
					| (__FDS_BITS(&writefds_tcp))[i];
		}
		ret_val = count_dtp + count_tcp;
    }	
    else {
		for (i = 0; i < maxidx; i++) {
			if (readfds)
				(__FDS_BITS(readfds))[i] = (__FDS_BITS(&readfds_tcp))[i];
			if (writefds)
				(__FDS_BITS(writefds))[i] = (__FDS_BITS(&writefds_tcp))[i];
		}
		/*
		if (readfds)
			(*readfds) = readfds_tcp;
		if (writefds)
			(*writefds) = writefds_tcp;
		*/
		ret_val = count_tcp;
    }

    TRACE("dtp_select() end (%d)\n", ret_val);
    return ret_val;
}
/*-------------------------------------------------------------------*/
int 
dtp_poll (struct pollfd *fds, unsigned nfds, int timeout)
{
    fd_set_s rdset[1], wrset[1], exset[1];
    struct timeval tv = { 0, 0 };
    int val = -1;
	unsigned i;

    FD_ZERO_S (rdset);
    FD_ZERO_S (wrset);
    FD_ZERO_S (exset);
    for (i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        if (val < fd)
            val = fd;

        if ((unsigned)fd >= FD_SETSIZE_S)
        {
            errno = EINVAL;
            return -1;
        }
        if (fds[i].events & POLLIN)
            FD_SET_S (fd, rdset);
        if (fds[i].events & POLLOUT)
            FD_SET_S (fd, wrset);
    }

    if (timeout >= 0)
    {
        div_t d = div (timeout, 1000);
        tv.tv_sec = d.quot;
        tv.tv_usec = d.rem * 1000;
    }

	/* FIXME: Current DTP does not support erset */
    val = dtp_select (val + 1, (fd_set *)rdset, (fd_set *)wrset, NULL,
                  (timeout >= 0) ? &tv : NULL);
    if (val == -1) {
        return -1;
	}

    for (i = 0; i < nfds; i++)
    {
        int fd = fds[i].fd;
        fds[i].revents = (FD_ISSET_S (fd, rdset) ? POLLIN : 0)
            | (FD_ISSET_S (fd, wrset) ? POLLOUT : 0);

    }
    return val;
}
/*-------------------------------------------------------------------*/
int
dtp_fcntl(dtp_socket_t socket, int cmd, int arg)
{
    dtp_context* ctx;
    ctx = DTPGetContextBySocket(socket);

    /* if socket is not opened, return EBADF */
    if (ctx == NULL || !ctx->tc_isActive) {
		errno = EBADF;
		return -1;
    }

    switch (cmd) {
    case DTP_F_GETFL:
		return (ctx->tc_fstatus);

    case DTP_F_SETFL:
		ctx->tc_fstatus = arg;
		return 0;

    default:
		TRACE_ERR("Currently, dtp_fcntl() supports only DTP_F_GETFL and DTP_F_SETFL\n");
		return -1;
    }
    
    /* should not happen */
    ASSERT(0, );
    return 0;
}
/*-------------------------------------------------------------------*/
int
dtp_getsockopt(dtp_socket_t socket, int level, int optname,
			   void *optval, socklen_t *optlen)
{
    dtp_context* ctx;
    ctx = DTPGetContextBySocket(socket);

    if (ctx == NULL || !ctx->tc_isActive) {
		errno = EBADF;
		return -1;
    }

    switch (optname) {
    case DTP_SO_ACCEPTCONN:
		return (ctx->tc_state == SOCK_LISTEN);
	
    case DTP_SO_RCVBUF:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_readBufLen), *optlen);
		break;
	
    case DTP_SO_SNDBUF:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_writeBufLen), *optlen);
		break;

    case DTP_SO_KEEPALIVE:
		*optlen = sizeof(int);
		int value = (ctx->tc_isKeepAliveEnabled)? TRUE : FALSE;		
		memcpy(optval, &(value), *optlen);
		break;

	case DTP_KEEPIDLE:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_keepAliveTime), *optlen);
		break;

	case DTP_KEEPINTVL:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_keepAliveIntvl), *optlen);
		break;

	case DTP_KEEPCNT:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_keepAliveProbes), *optlen);
		break;

	case DTP_SO_DEADLINE:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_deadline), *optlen);
		break;

	case DTP_SO_BLOCKSIZE:
        *optlen = sizeof(int);
        memcpy(optval, &(ctx->tc_blockSize), *optlen);
        break;

    case DTP_SO_FLOWID:
        *optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_flowID), *optlen);
        break;

    default:
		TRACE("Currently, dtp_getsockopt() supports only DTP_SO_ACCEPTCONN, DTP_SO_RCVBUF, DTP_SO_SNDBUF.\n");
		errno = ENOPROTOOPT;
		return -1;
    }

    return 0;
}
/*-------------------------------------------------------------------*/
int
dtp_setsockopt(dtp_socket_t socket, int level, int optname,
			   const void *optval, socklen_t optlen)
{
    u_char* ptr;
    dtp_context* ctx;
    struct timeval tv;
    ctx = DTPGetContextBySocket(socket);
    if (ctx == NULL || !ctx->tc_isActive) {
		errno = EBADF;
		return -1;
    }

    switch (optname) {
    case DTP_SO_ACCEPTCONN:
		TRACE_ERR("DTP_SO_ACCEPTCONN is invalid on setsockopt().\n");
		errno = EINVAL;
		return -1;

    case DTP_SO_RCVBUF:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}

		if (ctx->tc_rcvLoBufHB > *((int*)optval)) {
			errno = EINVAL;
			return -1;
		}
		/* double it to allow space for bookkeeping overhead */
		memcpy(&(ctx->tc_readBufLen), optval, optlen);
		/* ctx->tc_readBufLen *= 2; */

		/* reallocate the read buffer. */
		/* FIX : what if there exists data in buffer */
		/* (now just assume setsockopt is called before the data transfer.) */
		ptr = (u_char *)realloc(ctx->tc_readBuf, ctx->tc_readBufLen);
		if (ptr == NULL) {
			FREE_MEM(ctx->tc_readBuf);
			TRACE("failed to reallocate read buffer.\n");
			errno = ENOMEM;
			return -1;
		}
		ctx->tc_readBuf = ptr;
		break;
	
    case DTP_SO_SNDBUF:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}

		/* double it to allow space for bookkeeping overhead */
		memcpy(&(ctx->tc_writeBufLen), optval, optlen);
		ctx->tc_writeBufLen *= 2;

		/* reallocate the write buffer. */
		/* FIX : what if there exists data in buffer */
		/* (now just assume setsockopt is called before the data transfer.) */
		ptr = (u_char *)realloc(ctx->tc_writeBuf, ctx->tc_writeBufLen);
		if (ptr == NULL) {
			FREE_MEM(ctx->tc_writeBuf);
			TRACE("failed to reallocate write buffer.\n");
			errno = ENOMEM;
			return -1;
		}
		ctx->tc_writeBuf = ptr;
		break;

    case DTP_SO_KEEPALIVE:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		ctx->tc_isKeepAliveEnabled = (*(int*)optval != 0) ? TRUE : FALSE;
		break;

	case DTP_KEEPIDLE:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		memcpy(&(ctx->tc_keepAliveTime), optval, optlen);
		break;

	case DTP_KEEPINTVL:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		memcpy(&(ctx->tc_keepAliveIntvl), optval, optlen);
		break;

	case DTP_KEEPCNT:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		memcpy(&(ctx->tc_keepAliveProbes), optval, optlen);
		break;

    case DTP_SO_LINGER:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		ctx->tc_useLinger = (*(int*)optval != 0) ? TRUE : FALSE;
        break;

	case DTP_SO_BINDTODEVICE:
		return setsockopt(ctx->tc_sock, SOL_SOCKET, SO_BINDTODEVICE,
				optval, optlen);

		//#ifdef IN_MOBILE
		/* block size */
    case DTP_SO_BLOCKSIZE:

		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		memcpy(&ctx->tc_blockSize, optval, optlen);
		LOGD("1. BLOCKSIZE SET %u", ctx->tc_blockSize);

		ctx->tc_blockRemain += ctx->tc_blockSize;
		ctx->tc_lastBlockRemain = ctx->tc_blockRemain;

        if (ctx->tc_deadline > 0 && ctx->tc_blockSize > 0) {
            ctx->tc_isDeadlineSet = TRUE;
            LOGD("DEADLINE SET = %d", ctx->tc_deadline);
			gettimeofday(&tv, NULL);
			ctx->tc_deadlineTime.tv_sec = tv.tv_sec + ctx->tc_deadline;
			ctx->tc_deadlineTime.tv_usec = tv.tv_usec;
        }

        double now;
        if (gettimeofday(&tv, NULL)) {
            perror("gettimeofday() failed");
			EXIT(-1, );
        }
        now = tv.tv_sec + (tv.tv_usec / 1e6);
        ctx->tc_schedAllowTime = 0;
        ctx->tc_networkTimeLast = now;
        ctx->tc_blockRemainLast = ctx->tc_blockRemain;

		RemoveFromRetransQueue(ctx);
		InsertToRetransQueue(ctx);
		break;
		//#endif /* IN_MOBILE */

		/* deadline per each block */
    case DTP_SO_DEADLINE:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		memcpy(&ctx->tc_deadline, optval, optlen);
		
		/*
		if (ctx->tc_isDeadlineSet == TRUE && ctx->tc_isDeadlineChanged == FALSE)
			ctx->tc_isDeadlineChanged = TRUE;
		*/
        if (ctx->tc_deadline > 0 && ctx->tc_blockSize > 0) {
            ctx->tc_isDeadlineSet = TRUE;
            LOGD("DEADLINE SET = %d", ctx->tc_deadline);
			gettimeofday(&tv, NULL);
			ctx->tc_deadlineTime.tv_sec = tv.tv_sec + ctx->tc_deadline;
			ctx->tc_deadlineTime.tv_usec = tv.tv_usec;
        }
		else if (ctx->tc_deadline == 0) {
			ctx->tc_useMobile = TRUE;
		}
		else {
			/* ctx->tc_deadline < 0 : impossible! */
			errno = EINVAL;
			return -1;
		}
		RemoveFromRetransQueue(ctx);
		InsertToRetransQueue(ctx);

		break;

	case DTP_SO_WIFIONLY:
		if (optlen != sizeof(int)) {
			errno = EINVAL;
			return -1;
		}
		if (*(int*)optval != 0) {
			ctx->tc_isWifiOnly = TRUE;
			ctx->tc_useMobile = FALSE;		
		}
		else {
			ctx->tc_isWifiOnly = FALSE;
			ctx->tc_useMobile = TRUE; // XXX : THIS LINE NEEDS TO BE CHECKED		
		}
		break;

	case DTP_SO_RCVLOBUF:
		if (optlen != sizeof(struct dtp_rcvlobuf)) {
			errno = EINVAL;
			return -1;
		}

		uint32_t highBound = ((struct dtp_rcvlobuf *)optval)->high_bound;
		uint32_t lowBound = ((struct dtp_rcvlobuf *)optval)->low_bound;

		if (highBound > ctx->tc_readBufLen || lowBound >= highBound) {
			errno = EINVAL;
			return -1;
		}

		ctx->tc_rcvLoBufHB = highBound;
		ctx->tc_rcvLoBufLB = lowBound;

		break;

	case DTP_NODELAY:
		break;

    default:
		TRACE_ERR("only DTP_SO_RCVBUF, DTP_SO_SNDBUF, DTP_SO_KEEPALIVE supported\n");
		errno = ENOPROTOOPT;
		return -1;
    }

    return 0;    
}

/*
 * argument dest must be in network order.
 */
int
dtp_getifacename(uint32_t dest, char *buf, size_t len)
{
	FILE *fp;
	char line[256] = {0};
	char *token;
	int isFirst = 1;
	int cap_route = 5, len_route = 0;
	int iface = 0, destination = 0, mask = 0;
	int i, top = -1;
	uint32_t top_mask = 0;
	void *tmp;

	struct _route {
		char *name;
		uint32_t dest;
		uint32_t mask;
	} *route;
	route = (struct _route *)malloc(cap_route * sizeof(struct _route));
	if (route == NULL) {
		perror("malloc failure");
		return -1;
	}

	fp = fopen("/proc/net/route", "r");
	if (fp == NULL) {
		perror("open failure");
		free(route);
		return -1;
	}

	if (fgets(line, 255, fp) == NULL) {
		perror("cannot read file");
		free(route);
		fclose(fp);
		return -1;
	}

	for (i = 1; 
		 (token = strtok(isFirst ? line : NULL, " \t\n")) != NULL;
		 i++) {
		isFirst = 0;
		if (strcmp(token, "Iface") == 0)
			iface = i;
		else if (strcmp(token, "Destination") == 0)
			destination = i;
		else if (strcmp(token, "Mask") == 0)
			mask = i;
	}

	if (iface * destination * mask == 0) {
		perror("insufficient arguments");
		free(route);
		fclose(fp);
		return -1;
	}

	while(fgets(line, 255, fp) != NULL) {
		isFirst = 1;
		if (len_route >= cap_route) {
			cap_route *= 2;
			tmp = realloc(route, cap_route * sizeof(struct _route));
			if (tmp == NULL) {
				for (i = 0; i < len_route; i++)
					if (route[i].name != NULL)
						free(route[i].name);
				free(route);
				perror("realloc failure");
				fclose(fp);
				return -1;
			}
			else
				route = (struct _route *)tmp;
		}
		for (i = 1;
			 (token = strtok(isFirst ? line : NULL, " \t\n")) != NULL;
			 i++) {
			isFirst = 0;
			if (i == iface) {
				route[len_route].name = (char *)malloc(strlen(token)+1);
				strcpy(route[len_route].name, token);
			}
			else if (i == destination)
				sscanf(token, "%x", &route[len_route].dest);
			else if (i == mask)
				sscanf(token, "%x", &route[len_route].mask);
		}
		len_route++;
	}

	fclose(fp);

	for (i = 0; i < len_route; i++)
		// NOTE: Byte order does not have to be concerned
		// TODO: We have to concern the existance of same prefix length
		if (((route[i].dest ^ dest) & route[i].mask) == 0
			&& top_mask <= route[i].mask) {
			top = i;
			top_mask = route[i].mask;
		}

	if (top == -1) {
		perror("cannot find route");
		free(route);
		return -1;
	}

	if (route[top].name != NULL && buf != NULL)
		strncpy(buf, route[top].name,
				strlen(route[top].name) < len ? strlen(route[top].name) : len);
	else {
		perror("cannot read interface name");
		free(route);
		return -1;
	}

	for (i = 0; i < len_route; i++)
		if (route[i].name != NULL)
			free(route[i].name);
	free(route);

	return 0;
}

/*-------------------------------------------------------------------*/
int
dtp_ioctl(dtp_socket_t socket, int request, ...) 
{
	int value = 0;
	dtp_context* ctx;

	ctx = DTPGetContextBySocket(socket);
	if (ctx == NULL || !ctx->tc_isActive) {
		TRACE("invalid socket descriptor");
		errno = EBADF;
		return -1;
	}

	switch (request) {
#if 0
		/* FIOSNBIO is for HP-UX */
		case FIOSNBIO:
			ctx->tc_fstatus |= O_NONBLOCK;
			break;
#endif
		case FIONREAD:

			/* connection timed out */
			if (ctx->tc_connTimedOut) {
				errno = ETIMEDOUT;
				return -1;
			}
			/* connection reset */
			if (ctx->tc_connReset) {
				errno = ECONNRESET;
				return -1;
			}

			/* acquire a lock to access the shared variables */
			if (pthread_mutex_lock(&ctx->tc_readBufLock)) {
				TRACE_ERR("pthread_mutex_lock() failed");
				return -1;
			}

			value = GetReadBufDataSize(ctx);

			if (pthread_mutex_unlock(&ctx->tc_readBufLock)) {
				TRACE_ERR("pthread_mutex_unlock() failed");
				return -1;
			}

			break;

		default:
			return -1;
	}
	
    return value;
}

void dtp_printstat(int sock)
{
	dtp_context* ctx;

	ctx = DTPGetContextBySocket(sock);
	if (ctx == NULL || !ctx->tc_isActive) {
		printf("invalid socket descriptor\n");
		errno = EBADF;
		return;
	}

	DTPPrintCtx(stdout, ctx);
	
	return;
}

#define MAX_CPULOCK_INTERVAL 180
#define RETRY_INTERVAL		 10

int
dtp_getlocktime()
{
	int locktime, locktime_new;
	double diff, now;
	double req_time;
    struct timeval tv;
    dtp_context *ctx;
	char temp;

	LOGD("dtp_getlocktime start");

	/* return default if no way route */
	if (dtp_getifacename(0, &temp, 1) == -1)
		return -MAX_CPULOCK_INTERVAL;

	if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, return -RETRY_INTERVAL);
    }
    now = tv.tv_sec + (tv.tv_usec / 1e6);

	locktime = -MAX_CPULOCK_INTERVAL;

	LOGD("dtp_getlocktime loop start");
    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
		
		if (ctx->tc_isDeadlineSet == FALSE) {
			LOGD("dtp_getlocktime end 1");
			return MAX_CPULOCK_INTERVAL;
		}
		else {
			if (ctx->tc_mobileSpeed == 0) {
				return MAX_CPULOCK_INTERVAL;
			}
			req_time = ((double)ctx->tc_blockRemain)
				/ ((double)ctx->tc_mobileSpeed);	
			diff = ctx->tc_deadlineTime.tv_sec +
				(ctx->tc_deadlineTime.tv_usec / 1e6) - now;

			locktime_new = (req_time - diff < 0)? (req_time - diff) : diff;
			locktime = MAX(locktime, locktime_new);

			if (locktime >= MAX_CPULOCK_INTERVAL) {
				LOGD("dtp_getlocktime end 2");
				return MAX_CPULOCK_INTERVAL;
			}
		}
	}

	LOGD("dtp_getlocktime end 3");
	return locktime;
}
