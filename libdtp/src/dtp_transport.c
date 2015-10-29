#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <event2/event.h>
#include <event2/thread.h>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include "dtp_transport.h"
#include "dtp_select.h"
#include "dtp.h"
#include "crypt.h"
#include "debug.h"
#include "context.h"
#include "dtp_mobile.h"
#include "dtp_retrans_queue.h"
#include "queue.h"
#include "dtp_socket.h"

static int networkTypeLast = 0;
static uint32_t ipaddrLast = 0;


#include <netinet/in.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#include "dhkim_debug.h"

#define DELAY_ACK_TIMEOUT 200      /* 200 milliseconds for delayed ACK */
#define DEF_READ_BUFLEN  (32*1024) /* notice: win_size is 16 bit */
#define DEF_WRITE_BUFLEN (32*1024)
#define DTPHDRSIZE  (sizeof(struct dtp_hdr))
#define ALPHA 0.125
#define BETA 0.25
#define SENDTO 0
#define SENDMSG 1

#define RETRY_INTVL 1

/* delayed ACK queue */
static TAILQ_HEAD(/* not used */, dtp_context) g_delayedAckQHead = 
    TAILQ_HEAD_INITIALIZER(g_delayedAckQHead);

static RSA *g_rsa = NULL;                   /* RSA key structure */

/*-------------------------------------------------------------------*/
/* for batch procesing received packets in OnReadEvent */
#define MAX_BATCH 8
typedef struct adjPktInfo {
    dtp_context *ai_ctx;
    uint32_t    ai_maxACK;
    uint16_t    ai_windowSize;
    u_char     *ai_pkt[MAX_BATCH];
    int         ai_len[MAX_BATCH];
    int         ai_count; // number of packets
    int         ai_hasACK;
} AdjPktInfo;
/*-------------------------------------------------------------------*/

static void DTPSlowStart(dtp_context *ctx);
void HandleDisconnect(dtp_context *ctx);
//static void DTPReconnect(dtp_context* ctx, int delay);
//static void DTPRetransmit(int sock, dtp_pkt *packet);

#define ADD_STR(dst, src) strcpy(&dst[strlen(dst)], src)
#define ADD_NSTR(dst, src, len) strncpy(&dst[strlen(dst)], src, len)

int
dtp_setiface(uint32_t dest, char *buf, size_t len)
{
	/* XXX: Should be this method atomic?
	 * Is there any possibility of transmission failure while
	 * this function is in progress? */
	char command[256] = {0};
	char ifacename[64] = {0};
	struct in_addr dst_addr;
	dst_addr.s_addr = (in_addr_t)dest;

	/* FIXME: inet_ntoa is not thread-safe. */
	char *ip = inet_ntoa(dst_addr);

	if (!dtp_getifacename(dest, ifacename, sizeof(ifacename) - 1)) {
		LOGD("iface = %s", ifacename);
		/* FIXME: Using strcmp is not safe.
		 * Replace it with strncmp */
		if (strcmp(buf, ifacename) == 0)
			return 0;

		/* dest is on routing table. */
		ADD_STR(command, "su -c \"ip route del ");
		ADD_STR(command, ip);
		ADD_STR(command, "/24 dev ");
		ADD_STR(command, ifacename);
		ADD_STR(command, "\"");

		int res = system(command);
		LOGD("res = %d", res);
	}

	memset(command, 0, sizeof(command));

	ADD_STR(command, "su -c \"ip route add ");
	ADD_STR(command, ip);
	ADD_STR(command, "/24 dev ");
	ADD_NSTR(command, buf, len);
	ADD_STR(command, "\"");

	int res2 = system(command);
	LOGD("res2 = %d", res2);

	return dtp_getifacename(dest, ifacename, sizeof(ifacename));
}

void
DTPReadBufWakeup (dtp_context *ctx) {
	//assert(ctx);
	ASSERT(ctx, return);
	if (pthread_mutex_lock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
	}
	if (ctx->tc_isReadBlocked)
		if (pthread_cond_signal(&ctx->tc_readBufCond)) {
			TRACE("pthread_cond_signal failed\n");
			EXIT(-1, );
		}

	DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ);

	if (pthread_mutex_unlock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, return);
	}
}

void
DTPWriteBufWakeup (dtp_context *ctx) {

	ASSERT(ctx, return);
	if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
	}
	if (ctx->tc_isWriteBlocked)
		if (pthread_cond_signal(&ctx->tc_writeBufCond)) {
			TRACE("pthread_cond_signal failed\n");
			EXIT(-1, );
		}

	DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);

	if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, return);
	}
}

/*-------------------------------------------------------------------*/
dtp_context*
DTPCreateContext() 
{
    dtp_context* new_ctx;
    int new_sock;

    /* create a UDP socket */
    new_sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (new_sock < 0) {
		TRACE_ERR("socket() failed");		
		return NULL;
    }
    /* set the socket non-blocking */
    if (fcntl(new_sock, F_SETFL, O_NDELAY) == -1) {
		TRACE_ERR("fcntl() failed");
		close(new_sock);
		return NULL;
    }

    TRACE("created new physical socket: %d\n", new_sock);

    new_ctx = DTPAllocateContext(new_sock); 
	if (new_ctx == NULL) {
		errno = ENOMEM;
		close(new_sock);
		return NULL;
	}
    memset(new_ctx, 0, sizeof(*new_ctx));

    new_ctx->tc_sock = new_sock;

	DHK_TFLOG(DHK_DEBUG & DTRANS, DHK_F_BASE"dtp_trans.txt",
			"new_fd=%d, new_ctx=0x%08lX, state=%d",
			new_ctx->tc_sock, (unsigned long)new_ctx, new_ctx->tc_state);

    DTPRegisterSockToGlobalFdSet(new_sock);

    /* mutex locks & conditions */
    if (pthread_mutex_init(&new_ctx->tc_readBufLock, NULL) ||
		pthread_mutex_init(&new_ctx->tc_writeBufLock, NULL) ||
		pthread_mutex_init(&new_ctx->tc_connTimeLock, NULL) ||
		pthread_mutex_init(&new_ctx->tc_upByteLock, NULL) ||
		pthread_mutex_init(&new_ctx->tc_downByteLock, NULL)) {
		TRACE_ERR("pthread_mutex_init() failed");
		close(new_sock);
		return NULL;
    }    
    if (pthread_cond_init(&new_ctx->tc_readBufCond, NULL) ||
		pthread_cond_init(&new_ctx->tc_writeBufCond, NULL) ||
		pthread_cond_init(&new_ctx->tc_closeCond, NULL)) {
		TRACE_ERR("pthread_cond_init() failed");
		close(new_sock);
		return NULL;
    }

	new_ctx->tc_state = SOCK_CLOSED;

    new_ctx->tc_isActive = TRUE;

    /* network connection */
    new_ctx->tc_isNetConnected = TRUE;

    /* start keep-alive timer */	
    if (gettimeofday(&new_ctx->tc_keepAliveStart, NULL)) {
		TRACE_ERR("gettimeofday() failed");
		close(new_sock);
		return NULL;
    }
	
    new_ctx->tc_keepAliveTime = DTP_KEEPALIVE_TIME;       // required idle time to send probes
	new_ctx->tc_keepAliveIntvl = DTP_KEEPALIVE_PROBES;    // interval time between probes
	new_ctx->tc_keepAliveProbes = DTP_KEEPALIVE_INTVL;    // number of probes to be sent

    /* initialize RSA structure */
    new_ctx->tc_keyLen = 0;

    /* delayed ACK */
    new_ctx->tc_numACKRcvd = 0;

    /* window scale */
    new_ctx->tc_useWindowScale = FALSE; /* disabled by default */
    new_ctx->tc_recvWindowScale = 0;
    new_ctx->tc_sendWindowScale = 0;

	/* file size */
	new_ctx->tc_blockRemain = 0;

	/* up/downlink byte count */
	new_ctx->tc_upByte = 0;
	new_ctx->tc_downByte = 0;

	new_ctx->tc_nonce = 0;
	
	// -- commented out, should be checked again (2013.06.19)
	// default deadline
	gettimeofday(&(new_ctx->tc_deadlineTime), NULL);
	new_ctx->tc_deadline = 0;

    /* use default value 4 */
    UseWindowScale(new_ctx, 4);

#ifdef IN_MOBILE
	DTPMobileContextInit(new_ctx);
#endif

    /* packet list */
    TAILQ_INIT(&new_ctx->tc_packetQHead);

	InsertToRetransQueue(new_ctx);

    /* read & write buffer */
    /* FIX : 1) default readBuf/writeBuf size,  2) malloc on use */
    new_ctx->tc_readBufLen = DEF_READ_BUFLEN;
    new_ctx->tc_readBuf = (u_char *)malloc(new_ctx->tc_readBufLen);
    if (new_ctx->tc_readBuf == NULL) {
		TRACE_ERR("malloc() failed");
		close(new_sock);
		return NULL;
    }
    new_ctx->tc_writeBufLen = DEF_WRITE_BUFLEN;
    new_ctx->tc_writeBuf = (u_char *)malloc(new_ctx->tc_writeBufLen);
    if (new_ctx->tc_writeBuf == NULL) {
		TRACE_ERR("malloc() failed");
		free(new_ctx->tc_readBuf);
		close(new_sock);
		return NULL;
    }

    /* slow start */
    DTPSlowStart(new_ctx);

	DHK_TFLOG(DHK_DEBUG & DTRANS, DHK_F_BASE"dtp_trans.txt",
			"new_ctx=0x%08lX confirmed",
			(unsigned long)new_ctx);
    return new_ctx;
}

/*-------------------------------------------------------------------*/
static void
DTPSlowStart(dtp_context* ctx)
{
    /* timeout timer */
    ctx->tc_estRTT = RTO;
    ctx->tc_devRTT = 0;
    ctx->tc_RTO = RTO;

    /* delayed ACK */
    ctx->tc_numPacketRcvd = 0;

    /* flow control */
    ctx->tc_rcvWindow = ctx->tc_readBufLen;

    /* congestion control */
    ctx->tc_isCongested = FALSE;
    ctx->tc_cwnd = DTP_MTU - (int)DTPHDRSIZE;
    ctx->tc_ssthresh = ctx->tc_readBufLen;
    ctx->tc_segSent = 0;
}
/*-------------------------------------------------------------------*/
static void
DTPInsertPacket(dtp_pkt* packet, int temp_len, int hdr_size)
{
    packet->tp_seqNum = packet->tp_ctx->tc_lastByteSent;
    packet->tp_len = temp_len;
    packet->tp_headerLen = hdr_size;
    if (gettimeofday(&packet->tp_time, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, );
    }
    packet->tp_isRetrans = FALSE;
}
/*-------------------------------------------------------------------*/
static void
DTPGeneratePacket(struct msghdr *mh, struct iovec *iov,
				  dtp_pkt *packet, u_char* buf)
{
    /* specify components of message */
    iov[0].iov_base = packet->tp_header;
    iov[0].iov_len = packet->tp_headerLen;
    iov[1].iov_base = buf;
    iov[1].iov_len = packet->tp_len - packet->tp_headerLen;
    
    /* parameters for message header */
    mh->msg_name = &packet->tp_ctx->tc_peerAddr;
    mh->msg_namelen = sizeof(packet->tp_ctx->tc_peerAddr);
    mh->msg_iov = iov;
    mh->msg_iovlen = 2;
    mh->msg_control = NULL;
    mh->msg_controllen = 0;
    mh->msg_flags = 0;
}
/*-------------------------------------------------------------------*/
static inline uint32_t GetFlowID(const u_char *p);
static inline uint32_t GetSeqNum(const u_char *p);
static inline uint32_t GetAckNum(const u_char *p);
static inline int IsSYNPacket(const u_char *p);
static inline int IsACKPacket(const u_char *p);
static inline int IsRSTPacket(const u_char *p);
static inline int IsFINPacket(const u_char *p);
static inline int IsCHGPacket(const u_char *p);
static inline int IsRSPPacket(const u_char *p);
static inline int IsAUTHPacket(const u_char *p);
/*-------------------------------------------------------------------*/
static int
DTPSendPacket(int sock, void *buf, int len, int flags, 
			  struct sockaddr_in* addr, socklen_t addrlen,
			  dtp_context *ctx)
{
#ifdef DHK_DEBUG
	void *ptr = flags == SENDMSG
		? (void *)((struct msghdr*)buf)->msg_iov[0].iov_base
		: buf;
	if (IsFINPacket(ptr))
		DHK_TFLOG(DHK_DEBUG & DFINPKT, DHK_F_BASE"/packet.txt",
				"fd=%d\t-> FIN   \t%s=%08X\tfid=%08X",
				sock,
				IsACKPacket(ptr)? "ack" : "seq",
				IsACKPacket(ptr) 
				? ntohl(GetAckNum(ptr)) 
				: ntohl(GetSeqNum(ptr)),
				ntohl(GetFlowID(ptr)));
#endif
	DHK_MEM(DHK_DEBUG & DPKTHDR, DHK_F_BASE"/packet.txt",
			ptr, sizeof(struct dtp_hdr));
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t-> %s\t%s=%08X\tfid=%08X",
			sock,
			IsSYNPacket(ptr)? (IsACKPacket(ptr)? "SYNACK" : "SYN   ") :
			IsRSTPacket(ptr)? "RST   " :
			IsFINPacket(ptr)? "FIN   " :
			IsCHGPacket(ptr)? "CHG   " :
			IsRSPPacket(ptr)? "RSP   " :
			IsAUTHPacket(ptr)? "AUTH  " :
			IsACKPacket(ptr)? "ACK   " :
			"Normal",
			IsACKPacket(ptr)? "ack" : "seq",
			IsACKPacket(ptr) 
			? ntohl(GetAckNum(ptr)) 
			: ntohl(GetSeqNum(ptr)),
			ntohl(GetFlowID(ptr)));

    int res = 0;
    
    while (1) {

		if (flags == SENDTO) {
			res = sendto(sock, buf, len, 0, 
						 (struct sockaddr *)addr, addrlen);
		}
		else if (flags == SENDMSG) {
			res = sendmsg(sock, buf, 0);
		}
    
		if (res == -1) {
			/* network is unreachable */
			if (errno == ENETUNREACH) {
				if (ctx && ctx->tc_isNetConnected) {
					//HandleDisconnect(ctx); //YGMOON
					break;
				}
			}
			else if (errno == EAGAIN) {
				continue;
			}
			else if (errno == EBADF) {
				/* NOTE: Since UDP socket inside dtp_socket is inner socket, 
				   it should not be closed by normal close() function.
				   But if this case occurs, we should wipe out the context. */
				
				if (ctx && ctx->tc_isActive) {
					/* remove context from retransmission queue */
					RemoveFromRetransQueue(ctx);

					/* close the context */
					DTPCloseContext(ctx);
				}
				return -1;
			}
			else {
				TRACE("peer_addr=%s peer_port=%d addr_len=%d\n", 
					  inet_ntoa(addr->sin_addr), 
					  ntohs(addr->sin_port),
					  addrlen);

				if (flags == SENDTO) {
					TRACE_ERR("sendto() failed sock=%d temp_len=%d\n", 
							  sock, len);
				}
				else if (flags == SENDMSG) {	    
					TRACE_ERR("sendmsg() failed sock=%d temp_len=%d\n", 
							  sock, len);
				}

				if (errno == EFAULT)
					TRACE_ERR("invalid user space address!\n");

				return -1;
			}
		} else if (res != len) {
			TRACE_ERR("error: asked to write %d bytes but wrote %d bytes\n",
				  len, res);
			return -1;
		}
		else
			break;
    }

    return res;
}
/*-------------------------------------------------------------------*/
void
DTPWriteAvail(int sock, dtp_context *ctx)
{
#define IS_SYN() ((flags) & DTP_FLAG_SYN)
#define MAX_PAYLOAD (DTP_MTU - (int)DTPHDRSIZE)

    int len = ctx->tc_lastByteWritten - ctx->tc_lastByteSent;
    int flags = 0;
    int res = 0;
    dtp_pkt *packet;
    struct msghdr mh;
    struct iovec iov[2];
    int maxpayload;

	if (ctx == NULL)
		return;

    /* check network status & whether timeout occured */
    if (!ctx->tc_isNetConnected || ctx->tc_isCongested)
		return;

    if (ctx->tc_sendSYN && ctx->tc_state != SOCK_ESTABLISHED) {
		if (ctx->tc_isSYNSent)
			return;
		flags |= DTP_FLAG_SYN;
    }
    else if (!ctx->tc_isFirstDataSent) {
		flags |= DTP_FLAG_ACK;
		ctx->tc_isFirstDataSent = TRUE;
    }

    if (ctx->tc_recvAnyData)
		flags |= DTP_FLAG_ACK; /* need to have ACK */
    if (ctx->tc_isAUTHSent) {
		flags |= DTP_FLAG_AUTH; /* need to have AUTH */
		ctx->tc_isAUTHSent = FALSE;
	}
#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

    /* flow & congestion control */
    len = __min(len, ctx->tc_rcvWindow - 
				(ctx->tc_lastByteSent - ctx->tc_lastByteAcked));
    len = __min(len, (int)ctx->tc_cwnd - (int)ctx->tc_segSent);

    while ((len >= MAX_PAYLOAD) || 
		   ((len > 0) && 
			((ctx->tc_lastByteWritten - ctx->tc_lastByteSent < MAX_PAYLOAD)||
			 ctx->tc_closeCalled || (ctx->tc_rcvWindow < MAX_PAYLOAD)))) {

		int temp_len = 0;
		int hdr_size = DTPHDRSIZE;
		u_char hid[SHA1_DIGEST_LENGTH];
		uint8_t* recvWindowScale_ptr = NULL;

		packet = DTPGetIdlePacket();
		ASSERT(packet, break);

		hdr_size = DTPGenerateHeader(packet->tp_header, 
									 ctx->tc_lastByteSent, 
									 ctx->tc_nextByteExpected, 
									 ctx->tc_flowID, flags,
									 GetAvailReadBufSize(ctx) >> 
									 ctx->tc_recvWindowScale);

		if (IS_SYN()) {
			/* host ID */
			DTPGenerateHostID(hid);
			hdr_size = DTPAddOptionToHeader(packet->tp_header, hdr_size,
											DTPOPT_HOST_ID, hid);

			/* send deadline value */			
			LOGD("D = %d", ctx->tc_deadline);
			if (ctx->tc_deadline > 0) {
				hdr_size = DTPAddOptionToHeader(packet->tp_header, 
												hdr_size,
												DTPOPT_DEADLINE,
												&ctx->tc_deadline);
			} 
			ctx->tc_isSYNSent = TRUE;
		}
	
		if (!ctx->tc_isFirstPacketSent) {
			/* window scale */
			if (ctx->tc_recvWindowScale > 0) {
				recvWindowScale_ptr = &(ctx->tc_recvWindowScale);
				hdr_size = DTPAddOptionToHeader(packet->tp_header, hdr_size,
												DTPOPT_WIN_SCALE, recvWindowScale_ptr);
			}
			ctx->tc_isFirstPacketSent = TRUE;
		}

		hdr_size = ((hdr_size + 3) & ~0x3);

		/* determine the max data payload size (option size is considered) */
		if (ctx->tc_state == SOCK_SYN_SENT) {
			temp_len = hdr_size + ctx->tc_keyLen;
		}
		else {
			maxpayload = DTP_MTU - hdr_size;
			temp_len = (len <= maxpayload) ? len : maxpayload;
			temp_len += hdr_size;
		}	

		/* insert packet into packet list */
		packet->tp_ctx = ctx;
		DTPInsertPacket(packet, temp_len, hdr_size);
		TAILQ_INSERT_TAIL(&ctx->tc_packetQHead, packet, tp_link);

		/* generate message to send */
		DTPGeneratePacket(&mh, iov, packet,
						  GetWriteBufLastByteSentPtr(packet->tp_ctx));

		TRACE("sending a packet\n");
#ifdef DEBUGX
		DTPPrintPacket(packet->tp_header, temp_len);
#endif

		/* send packet */
#ifdef IN_MOBILE
		if (IsAvailConnection(ctx))
			res = DTPSendPacket(ctx->tc_sock, &mh, temp_len, SENDMSG,
								&ctx->tc_peerAddr, sizeof(ctx->tc_peerAddr),
								ctx);
#else
		res = DTPSendPacket(ctx->tc_sock, &mh, temp_len, SENDMSG,
							&ctx->tc_peerAddr, sizeof(ctx->tc_peerAddr),
							ctx);
#endif
		if ((res == -1) && (errno == ENETUNREACH))
			return;
#if 0
		/* dhkim: TODO: some adequate condition should be here */
		if (ctx->tc_lastByteSent + ctx->tc_writeBufLen 
				> ctx->tc_lastByteWritten) {
			if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
				perror("pthread_mutex_lock() failed");
				exit(-1);
			}
//			DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
			if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
				perror("pthread_mutex_unlock() failed");
				exit(-1);
			}
//			DTPWriteBufWakeup(ctx);
		}
#endif
		flags &= (~DTP_FLAG_SYN);
		flags &= (~DTP_FLAG_ACK);
		flags &= (~DTP_FLAG_AUTH);
		ctx->tc_recvAnyData = FALSE;
		temp_len -= hdr_size;
		ctx->tc_segSent += temp_len;
		len -= temp_len;
		ctx->tc_lastByteSent += temp_len;

		if (ctx->tc_state == SOCK_SYN_SENT)
			break;
    }

    /* if dtp_close was called, send FIN after all data are sent */
	/* DOUBLE CHECK : isFINRcvd is added for handling close() by other side
	 while sendind any data */
	DHK_FDBG(DHK_DEBUG & DTRANS, DHK_F_BASE"/dtp_trans.txt",
			"Check whether sock %d (flow %08X) send FIN or not.\n"
			"tc_closeCalled = %d\n"
			"tc_isFINSent = %d\n"
			"tc_isFINRcvd = %d\n"
			"tc_lastByteSent = %u\n"
			"tc_lastByteWritten = %u",
			ctx->tc_sock,
			ctx->tc_flowID,
			ctx->tc_closeCalled,
			ctx->tc_isFINSent,
			ctx->tc_isFINRcvd,
			ctx->tc_lastByteSent,
			ctx->tc_lastByteWritten);
    if (ctx->tc_closeCalled && !ctx->tc_isFINSent && 
		(ctx->tc_isFINRcvd || (ctx->tc_lastByteSent == ctx->tc_lastByteWritten))) {
		DTPSendFINPacket(ctx);
	}

}
/*-------------------------------------------------------------------*/
void
DTPSendEventToLibThread(int sock, dtp_event *ev)
{
	DHK_FDBG(DHK_DEBUG & DDEADLOCK01, DHK_F_BASE"deadlock01.txt", " ");
    while (write(sock, ev, sizeof(*ev)) != sizeof(*ev)) {
		DHK_FDBG(DHK_DEBUG & DDEADLOCK01, DHK_F_BASE"deadlock01.txt", " ");
		DHK_FDBG(DHK_DEBUG & DWARNING, DHK_F_BASE"error.txt",
				"inner socket, %d, write failed. %d:%s",
				sock, errno, strerror(errno));
//		TRACE_ERR("write failed\n");
		usleep(1000);
		if (errno != EAGAIN)
			EXIT(-1, break);
    }
	DHK_FDBG(DHK_DEBUG & DDEADLOCK01, DHK_F_BASE"deadlock01.txt", " ");
}
/*-------------------------------------------------------------------*/
int
DTPSendACKPacket(dtp_context *ctx)
{
	if (ctx == NULL)
		return 0;

    u_char hdr[DTPHDRSIZE];
    socklen_t addrlen = sizeof(ctx->tc_peerAddr);
    int flags = DTP_FLAG_ACK;
    int hdr_size;
    int res = 0;

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

	/* initializing the bytes in hdr */
	memset(hdr, 0, DTPHDRSIZE);

    /* generate header */
    hdr_size = DTPGenerateHeader(hdr, ctx->tc_lastByteSent, 
								 GetAckNumber(ctx),
								 ctx->tc_flowID, flags, 
								 (GetAvailReadBufSize(ctx) >> 
								  ctx->tc_recvWindowScale));

    hdr_size = ((hdr_size + 3) & ~0x3);

    TRACE("about to send an ACK\n");
#ifdef DEBUGX
    DTPPrintPacket(hdr, hdr_size);
#endif

    /* send packet */
#ifdef IN_MOBILE
	if (IsAvailConnection(ctx)) {
		res = DTPSendPacket(ctx->tc_sock, hdr, hdr_size, SENDTO,
							&ctx->tc_peerAddr, addrlen, NULL);
	}
#else
    res = DTPSendPacket(ctx->tc_sock, hdr, hdr_size, SENDTO,
						&ctx->tc_peerAddr, addrlen, NULL);
#endif

    return res;
}
/*-------------------------------------------------------------------*/
void
DTPSendFINPacket(dtp_context *ctx)
{
    u_char hdr[DTPHDRSIZE];
    socklen_t addrlen = sizeof(ctx->tc_peerAddr);
    dtp_pkt *packet;
    int flags = DTP_FLAG_FIN | DTP_FLAG_ACK;
    int hdr_size;

    TRACE("DTPSendFINPacket() start\n");

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

	/* initializing the bytes in hdr */
	memset(hdr, 0, DTPHDRSIZE);

    /* generate header */
    hdr_size = DTPGenerateHeader(hdr, ctx->tc_lastByteSent, 
								 GetAckNumber(ctx),
								 ctx->tc_flowID, flags,
								 GetAvailReadBufSize(ctx));

    packet = DTPGetIdlePacket();
    ASSERT(packet, return);

    /* insert packet into packet list */
    memcpy(packet->tp_header, hdr, hdr_size); 
    packet->tp_ctx = ctx;
    DTPInsertPacket(packet, hdr_size, hdr_size);
    TAILQ_INSERT_TAIL(&ctx->tc_packetQHead, packet, tp_link);

    TRACE("about to send a FIN\n");
#ifdef DEBUGX
    DTPPrintPacket(packet->tp_header, hdr_size);
#endif

    /* send packet */
    DTPSendPacket(ctx->tc_sock, packet->tp_header, hdr_size, 
				  SENDTO, &ctx->tc_peerAddr, addrlen, ctx);
    
    /* FIN packet has data size 1 */
    ctx->tc_lastByteSent += 1;
    ctx->tc_lastByteWritten += 1;

    /* change context state */
    if ((ctx->tc_state == SOCK_ESTABLISHED) || 
		(ctx->tc_state == SOCK_SYN_SENT))
		ctx->tc_state = SOCK_FIN_WAIT_1;
    else if (ctx->tc_state == SOCK_CLOSE_WAIT)
		ctx->tc_state = SOCK_LAST_ACK;
   
	//    assert((ctx->tc_state == SOCK_FIN_WAIT_1) || 
	//		   (ctx->tc_state == SOCK_LAST_ACK));

    /* FIN is sent */
    ctx->tc_isFINSent = TRUE;

    TRACE("DTPSendFINPacket() end\n");
}
/*-------------------------------------------------------------------*/
void
DTPSendProbePacket(dtp_context *ctx)
{
    u_char hdr[DTPHDRSIZE];
    socklen_t addrlen = sizeof(ctx->tc_peerAddr);
    int flags = 0;
    int hdr_size;

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

	/* initializing the bytes in hdr */
	memset(hdr, 0, DTPHDRSIZE);

    /* generate header */
    hdr_size = DTPGenerateHeader(hdr, ctx->tc_lastByteSent, 
								 GetAckNumber(ctx),
								 ctx->tc_flowID, flags, 
								 (GetAvailReadBufSize(ctx) >> 
								  ctx->tc_recvWindowScale));

    hdr_size = ((hdr_size + 3) & ~0x3);

    TRACE("about to send an probe packet (null data)\n");
#ifdef DEBUGX
    DTPPrintPacket(hdr, hdr_size);
#endif

    /* send packet */
#ifdef IN_MOBILE
	if (IsAvailConnection(ctx))
		DTPSendPacket(ctx->tc_sock, hdr, hdr_size, SENDTO,
					  &ctx->tc_peerAddr, addrlen, NULL);
#else
	DTPSendPacket(ctx->tc_sock, hdr, hdr_size, SENDTO,
				  &ctx->tc_peerAddr, addrlen, NULL);
#endif
}

/*-------------------------------------------------------------------*/
void
DTPCloseContext(dtp_context *ctx) 
{
    int ret_val;
    ASSERT(ctx->tc_isActive, return);

    /* tell application that teardown process is done */
    if (ctx->tc_waitACK) {
		//assert(ctx->tc_lastByteSent == ctx->tc_lastByteAcked);
		if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
			TRACE_ERR("pthread_mutex_lock() failed");
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
					"pthread_mutex_lock() failed. socket %d", ctx->tc_sock);
			EXIT(-1, return);
		}
		if (pthread_cond_signal(&ctx->tc_closeCond)) {
			perror("pthread_cond_signal() failed");
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
					"pthread_cond_signal() failed. socket %d", ctx->tc_sock);
			EXIT(-1, );
		}
		if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
			TRACE_ERR("pthread_mutex_unlock() failed");
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
					"pthread_mutex_unlock() failed. socket %d", ctx->tc_sock);
			EXIT(-1, return);
		}
		return;
    }

	/* if it is in TIME_WAIT state, wait for 30 sec */
    if (ctx->tc_state == SOCK_TIME_WAIT)
		return;

    buffer *buf = ctx->tc_readBufHead;	
	buffer *temp = NULL;

    while (buf != NULL) {
		temp = buf;
    	buf = buf->next_buf;
    	AddToFreeBufferList(temp);

		/* BEFORE FIX
		  AddToFreeBufferList(buf);
		  buf = buf->next_buf;
		 */
    }

    ctx->tc_readBufHead = NULL;

	/* XXX: Bug
	 * This 'may' solve double free or memory corruption problem in
	 * below FREE_MEM()s */
#if 0
	__sync_synchronize ();
#endif

    FREE_MEM(ctx->tc_readBuf);
    FREE_MEM(ctx->tc_writeBuf);
    ctx->tc_isActive = FALSE;
    ctx->tc_closeCalled = FALSE;

    DTPRemoveContextFromFlowIDMap(ctx);

    /* destroy mutex locks & conditions */
    if (pthread_mutex_destroy(&ctx->tc_readBufLock)) {
		TRACE("[sock %d] destroying a read mutex lock failed\n",
			  ctx->tc_sock);
		EXIT(-1, );
    }
    if (pthread_mutex_destroy(&ctx->tc_writeBufLock)) {
		TRACE("[sock %d] destroying a write mutex lock failed\n",
			  ctx->tc_sock);
		EXIT(-1, );
    }
    if (pthread_mutex_destroy(&ctx->tc_connTimeLock)) {
		TRACE("[sock %d] destroying a connection timeout mutex lock failed\n",
			  ctx->tc_sock);
		EXIT(-1, );
    }
    if (pthread_mutex_destroy(&ctx->tc_upByteLock)) {
		TRACE("[sock %d] destroying a uplink byte count mutex lock failed\n",
			  ctx->tc_sock);
		EXIT(-1, );
    }
    if (pthread_mutex_destroy(&ctx->tc_downByteLock)) {
		TRACE("[sock %d] destroying a downlink byte count mutex lock failed\n",
			  ctx->tc_sock);
		EXIT(-1, );
    }
    if (pthread_cond_destroy(&ctx->tc_readBufCond)) {
		TRACE("destroying a read cond variable failed\n");
		EXIT(-1, );
    }
    if (pthread_cond_destroy(&ctx->tc_writeBufCond)) {
		TRACE("destroying a write cond variable failed\n");
		EXIT(-1, );
    }
    if (pthread_cond_destroy(&ctx->tc_closeCond)) {
		TRACE("destroying a close cond variable failed\n");
		EXIT(-1, );
    }

	if ((ctx->tc_state == SOCK_CLOSE_WAIT
				|| ctx->tc_state == SOCK_LAST_ACK
				|| ctx->tc_state == SOCK_FIN_WAIT_1
				|| ctx->tc_state == SOCK_FIN_WAIT_2
				|| ctx->tc_state == SOCK_TIME_WAIT))
		DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
				"fid=%08X: Trying to DTPSelectEventClr() in %d state",
				ctx->tc_flowID, ctx->tc_state);
    DTPSelectEventClr(ctx->tc_sock, DTP_FD_READ);
    DTPSelectEventClr(ctx->tc_sock, DTP_FD_WRITE);

    /* delete listen event */
    ret_val = event_del(ctx->tc_readEvent);
    if (ret_val != 0) {
		TRACE("event_del failed\n");
		DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
				"event_del failed. socket %d", ctx->tc_sock);
		EXIT(-1, );
    }
    
    DTPClearSockFromGlobalFdSet(ctx->tc_sock);

	DHK_FLOG(DHK_DEBUG & DTEMP, DHK_F_BASE"/dtp_close.txt",
			"UDP%08X", ctx->tc_flowID);

    /* close connection socket */
	/* dhkim: shutdown() before close() */
    shutdown(ctx->tc_sock, SHUT_RDWR);
    if (close(ctx->tc_sock) == -1) {
		TRACE_ERR("tc_sock close error");
		DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
				"Failed to close socket %d", ctx->tc_sock);
	}
		
    TRACE("DTPCloseContext() end\n");
}
/*-------------------------------------------------------------------*/
static int
DTPSetInterface(dtp_context *ctx, const char *name, int len)
{
	/* TODO:
	 * We need to use SO_BINDTODEVICE option of setsockopt().
	 * This function is unimplemented because android application
	 * cannot get permission to do setsockopt(, SO_BINDTODEVICE, , )
	 */
	return -1;
}
/*-------------------------------------------------------------------*/
static inline int
IsRSTPacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->rst);
}
/*-------------------------------------------------------------------*/
static inline int
IsSYNPacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->syn);
}
/*-------------------------------------------------------------------*/
static inline int
IsFINPacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->fin);
}
/*-------------------------------------------------------------------*/
static inline int
IsCHGPacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->chg);
}
/*-------------------------------------------------------------------*/
static inline int
IsRSPPacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->rsp);
}
/*-------------------------------------------------------------------*/
static inline int
IsAUTHPacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->auth);
}
/*-------------------------------------------------------------------*/
static inline uint32_t
GetFlowID(const u_char *p)
{
    /* we assume hdr is in network byte order */
    return (((struct dtp_hdr *)p)->fid);
}
/*-------------------------------------------------------------------*/
static inline int
IsACKPacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->ack);
}
/*-------------------------------------------------------------------*/
static inline int
GetHeaderLen(const u_char *p)
{
    return (((struct dtp_hdr *)p)->doff << 2);
}
/*-------------------------------------------------------------------*/
static inline int
GetPayloadLen(const u_char *p, int total)
{
    return (total - GetHeaderLen(p));
}
/*-------------------------------------------------------------------*/
static inline uint32_t
GetSeqNum(const u_char *p) 
{
    return (((struct dtp_hdr *)p)->seq);
}
/*-------------------------------------------------------------------*/
static inline uint32_t
GetAckNum(const u_char *p) 
{
    return (((struct dtp_hdr *)p)->ack_seq);
}
/*-------------------------------------------------------------------*/
static inline uint16_t
GetRcvdWindow(const u_char *p) 
{
    return (((struct dtp_hdr *)p)->window_size);
}
/*-------------------------------------------------------------------*/
static inline int
IsMobilePacket(const u_char *p)
{
    return (((struct dtp_hdr *)p)->mobile);
}
/*-------------------------------------------------------------------*/
RSA *
DTPInitializeRSAKey()
{
    /* initialize RSA key */
	if (g_rsa == NULL) {
		g_rsa = RSA_new();
		if (g_rsa == NULL) {
			TRACE("RSA_new error\n");
			EXIT(-1, return NULL);
		}
		
		/* generate RSA key */
		g_rsa = RSA_generate_key(RSA_LEN * 8, PUB_EXP, NULL, NULL);
		if (g_rsa == NULL) {
			TRACE("RSA_generate_key error\n");
			EXIT(-1, return NULL);
		}
	}

    return g_rsa;
}
/*-------------------------------------------------------------------*/
static void
DTPHandleKey(int sock, dtp_context *ctx, void *buf, int isResend)
{
    unsigned char temp_buf[RSA_LEN];
    RSA *rsa = NULL;
    int n_len, e_len;
    dtp_pkt *packet;
    struct msghdr mh;
    struct iovec iov[2];
    int flags = DTP_FLAG_SYN | DTP_FLAG_ACK;
    unsigned char encrypt[RSA_LEN];
    u_char hid[SHA1_DIGEST_LENGTH];
    int hdr_size;
    int res;
	u_short port;
    struct sockaddr_in new_address;
    int new_addressLen = sizeof(new_address);

    /* initialize RSA structure */
    rsa = RSA_new();
    if (rsa == NULL) {
        TRACE("RSA_new error\n");
        EXIT(-1, return);
    }

    /* public key, n */
    memcpy(&n_len, buf + GetHeaderLen(buf), 4);
    memcpy(temp_buf, buf + GetHeaderLen(buf) + 4, n_len);
    if ((rsa->n = BN_bin2bn(temp_buf, n_len, rsa->n)) == NULL) {
		TRACE("Bad n value\n");
		EXIT(-1, return);
    }

    /* public key, e */
    memcpy(&e_len, buf + GetHeaderLen(buf) + 4 + n_len, 4);
    memcpy(temp_buf, buf + GetHeaderLen(buf) + 8 + n_len, e_len);
    if ((rsa->e = BN_bin2bn(temp_buf, e_len, rsa->e)) == NULL) {
		TRACE("Bad e value\n");
		EXIT(-1, return);
    }

    /* extend read offset */
	if (!isResend) {
		ctx->tc_nextByteExpected += (8 + n_len + e_len);
		ctx->tc_lastByteRcvd += (8 + n_len + e_len);
		ctx->tc_lastByteRead += (8 + n_len + e_len);
	}

    /* generate HMAC key */
    spc_keygen(ctx->tc_key, 20);

    /* encrypt HMAC key with RSA public key */
    res = RSA_public_encrypt(20, ctx->tc_key, encrypt, 
							 rsa, RSA_PKCS1_OAEP_PADDING);
    if (res == -1) {
		TRACE("RSA_public_encrypt error occured.");
		EXIT(-1, return);
    }

    /* get idle packet */
    packet = DTPGetIdlePacket();
    ASSERT(packet, return);

	if (isResend)
		ctx->tc_lastByteSent -= res;

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

    /* getenerate header */
    hdr_size = DTPGenerateHeader(packet->tp_header,
								 ctx->tc_lastByteSent,
								 ctx->tc_nextByteExpected,
								 ctx->tc_flowID, flags,
								 GetAvailReadBufSize(ctx) >>
								 ctx->tc_recvWindowScale);
    /* host ID */
    DTPGenerateHostID(hid);
    hdr_size = DTPAddOptionToHeader(packet->tp_header, hdr_size,
									DTPOPT_HOST_ID, hid);
    
    /* send deadline value */
    if (ctx->tc_deadline > 0) {
		hdr_size = DTPAddOptionToHeader(packet->tp_header, 
										hdr_size,
										DTPOPT_DEADLINE,
										&ctx->tc_deadline);
    }   

    if (!ctx->tc_isFirstPacketSent) {
		/* window scale */
		if (ctx->tc_recvWindowScale > 0) {
			hdr_size = DTPAddOptionToHeader(packet->tp_header, hdr_size, DTPOPT_WIN_SCALE, 
											&(ctx->tc_recvWindowScale));
		}
		ctx->tc_isFirstPacketSent = TRUE;
    }

	/* notify my new port to other side (passive open) */
    getsockname(ctx->tc_sock, (struct sockaddr*)&new_address,
				(socklen_t*)&new_addressLen);
	port = ntohs(new_address.sin_port);
	hdr_size = DTPAddOptionToHeader(packet->tp_header, hdr_size, DTPOPT_PORT, 
									&(port));

    hdr_size = ((hdr_size + 3) & ~0x3);
	
    /* insert packet into packet list */
    packet->tp_ctx = ctx;
    DTPInsertPacket(packet, res + hdr_size, hdr_size);
    
    /* generate message to send */
    DTPGeneratePacket(&mh, iov, packet, encrypt);

    TRACE("sending a packet\n");
#ifdef DEBUGX
    DTPPrintPacket(packet->tp_header, packet->tp_len);
#endif
	
    /* send encrypted packet for HMAC key */
	/* *** USE OLD SOCKET TO SEND SYNACK and NOTIFY NEW PORT */
	DTPSendPacket(sock, &mh, res + hdr_size, SENDMSG,
				  &ctx->tc_peerAddr, sizeof(ctx->tc_peerAddr), ctx);

	ctx->tc_lastByteSent += res;
	ctx->tc_lastByteWritten = ctx->tc_lastByteSent;
}
/*-------------------------------------------------------------------*/
static void
MoveCongestionWindow(dtp_context *ctx, int acked)
{
    if (ctx->tc_cwnd < ctx->tc_ssthresh) {
		ctx->tc_cwnd += acked;
		acked -= ctx->tc_ssthresh;
    }
    if (acked > 0)
		ctx->tc_cwnd += acked * DTP_MTU / ctx->tc_cwnd;
    if (ctx->tc_cwnd > ctx->tc_writeBufLen)
		ctx->tc_cwnd = ctx->tc_writeBufLen;
}
/*-------------------------------------------------------------------*/
static void
CongestionAvoidance(dtp_context *ctx)
{
    ctx->tc_ssthresh = ctx->tc_cwnd / 2;
    if (ctx->tc_ssthresh < DTP_MTU - (int)DTPHDRSIZE)
		ctx->tc_ssthresh = DTP_MTU - (int)DTPHDRSIZE;
    ctx->tc_cwnd = ctx->tc_ssthresh + 3 * (DTP_MTU - (int)DTPHDRSIZE);
}
/*-------------------------------------------------------------------*/
static void
TimeoutSlowStart(dtp_context *ctx)
{
    ctx->tc_ssthresh = ctx->tc_cwnd / 2;
    if (ctx->tc_ssthresh < DTP_MTU - (int)DTPHDRSIZE)
		ctx->tc_ssthresh = DTP_MTU - (int)DTPHDRSIZE;
    ctx->tc_cwnd = DTP_MTU - (int)DTPHDRSIZE;
}
/*-------------------------------------------------------------------*/
static double
CalculateRTO(dtp_pkt *packet)
{
    struct timeval tv;
    double rtt, now;
    dtp_context *pctx = packet->tp_ctx;

    /* recalculate RTT */
    if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, return 0);
    }

    now = tv.tv_sec + (tv.tv_usec / 1e6);

    rtt = now - packet->tp_time.tv_sec - (packet->tp_time.tv_usec / 1e6);
    pctx->tc_estRTT = ((1 - ALPHA) * pctx->tc_estRTT) + (ALPHA * rtt);
    pctx->tc_devRTT = ((1 - BETA) * pctx->tc_devRTT) + 
		(BETA * fabs(rtt - pctx->tc_estRTT));
    
    return (pctx->tc_estRTT + (4 * pctx->tc_devRTT));
}
/*-------------------------------------------------------------------*/
static void
DTPReconnect(dtp_context* ctx, int delay)
{
    dtp_pkt *packet;

    ctx->tc_isNetConnected = TRUE;

    /* check whether there was any probe packet */
    packet = TAILQ_FIRST(&ctx->tc_packetQHead);

    /* send ACK if there was no probe packet */
    if (packet == NULL) {
		/* FIX: handle packet drop */
		DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
		DTPSendACKPacket(ctx);
    }
    else {
		/* reset sent time for probing packet */
		if (gettimeofday(&packet->tp_time, NULL)) {
			perror("gettimeofday() failed");
			EXIT(-1, );
		}
	
		/* delay for reconnection used by wifievent */
		// packet->tp_time.tv_sec += delay;
	
		/* first packet is already sent */
		DTPSlowStart(ctx);
		ctx->tc_segSent = packet->tp_len;
    }
}
/*-------------------------------------------------------------------*/
static void
DTPRetransmit(int sock, dtp_pkt *packet)
{
    int res;
    dtp_context *pctx = packet->tp_ctx;
    struct msghdr mh;
    struct iovec iov[2];

    TRACE("DTPRetransmit() start\n");

#ifdef IN_MOBILE
	if (IsMobileConnected())
		((struct dtp_hdr *)(packet->tp_header))->mobile = 1;
#endif

    /* generate message to send */
	DTPGeneratePacket(&mh, iov, packet,
					  GetWriteBufPacketSentPtr(packet));
    ASSERT(packet->tp_seqNum == htonl(GetSeqNum(packet->tp_header)), );
	
#ifdef DEBUGX
    DTPPrintPacket(packet->tp_header, packet->tp_len);
#endif
    
#ifdef DHK_DEBUG
	void *ptr = (void *)mh.msg_iov[0].iov_base;
#endif
	DHK_TFLOG(DHK_DEBUG & DRETRANSMIT, DHK_F_BASE"retransmit.txt",
			"fd=%d\t-> %s\t%s=%08X\tfid=%08X",
			sock,
			IsSYNPacket(ptr)? (IsACKPacket(ptr)? "SYNACK" : "SYN   ") :
			IsRSTPacket(ptr)? "RST   " :
			IsFINPacket(ptr)? "FIN   " :
			IsCHGPacket(ptr)? "CHG   " :
			IsRSPPacket(ptr)? "RSP   " :
			IsAUTHPacket(ptr)? "AUTH  " :
			IsACKPacket(ptr)? "ACK   " :
			"Normal",
			IsACKPacket(ptr)? "ack" : "seq",
			IsACKPacket(ptr) 
			? ntohl(GetAckNum(ptr)) 
			: ntohl(GetSeqNum(ptr)),
			ntohl(GetFlowID(ptr)));

    /* send packet */
    res = DTPSendPacket(sock, &mh, packet->tp_len, SENDMSG, 
						&pctx->tc_peerAddr, sizeof(pctx->tc_peerAddr), 
						pctx);
    if ((res == -1) && (errno == ENETUNREACH)) {
		TRACE("DTPRetransmit() end\n");
		return;
    }

    /* network is reconnected */
    if (!pctx->tc_isNetConnected) {
		TRACE("network is reconnected\n");
		DTPReconnect(pctx, 0);
    }
    else
		packet->tp_isRetrans = TRUE;

    TRACE("DTPRetransmit() end\n");
}
/*-------------------------------------------------------------------*/
static void
SendRSTPacket(int sock, u_char *buf, struct sockaddr_in* addr)
{
    u_char hdr[DTPHDRSIZE + (DTPOLEN_HOST_ID + 4)] = {0};
    int flags = DTP_FLAG_RST;
    u_char hid[SHA1_DIGEST_LENGTH] = {0};
    int hdr_size = 0;

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

    /* generate header */
    hdr_size = DTPGenerateHeader(hdr, 0, 0, ntohl(GetFlowID(buf)), flags,
								 DEF_READ_BUFLEN);

    /* host ID */
    DTPGenerateHostID(hid);
    hdr_size = DTPAddOptionToHeader(hdr, hdr_size,
									DTPOPT_HOST_ID, hid);
    hdr_size = ((hdr_size + 3) & ~0x3);

    TRACE("about to send RST packet\n");
#ifdef DEBUGX
    DTPPrintPacket(hdr, hdr_size);
#endif
    
    /* send packet */    
    DTPSendPacket(sock, hdr, hdr_size, SENDTO, addr,
				  sizeof(struct sockaddr_in), NULL);
}
/*-------------------------------------------------------------------*/
static void
SendCHGPacket(dtp_context *ctx, struct sockaddr_in* addr)
{
    dtp_pkt *packet;
    int flags = DTP_FLAG_CHG;
    int hdr_size;
    struct msghdr mh;
    struct iovec iov[2];

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

    /* get idle packet */
    packet = DTPGetIdlePacket();
    ASSERT(packet, return);

    /* generate header */
    hdr_size = DTPGenerateHeader(packet->tp_header, 0, 0, 
								 ctx->tc_flowID, flags,
								 (GetAvailReadBufSize(ctx) >>
								  ctx->tc_recvWindowScale));
    hdr_size = ((hdr_size + 3) & ~0x3);

    /* generate nonce */
	if (ctx->tc_nonce == 0) {
		ctx->tc_nonce = rand() % MAX_INT + 1;
	}

    /* generate packet */
    packet->tp_ctx = ctx;
    DTPInsertPacket(packet, hdr_size + sizeof(ctx->tc_nonce), hdr_size);
    DTPGeneratePacket(&mh, iov, packet, (void*)&ctx->tc_nonce);
    
    /* change msghdr's address to new address */
    mh.msg_name = addr;

    /* send CHG packet with nonce */
    TRACE("about to send CHG packet\n");
#ifdef DEBUGX
    DTPPrintPacket(packet->tp_header, packet->tp_len);
#endif

    DTPSendPacket(ctx->tc_sock, &mh, hdr_size + sizeof(ctx->tc_nonce),
				  SENDMSG, addr, sizeof(struct sockaddr_in), ctx);

    /* remove packet */
    AddToFreePacketList(packet);
}
/*-------------------------------------------------------------------*/
static void
SendRSPPacket(dtp_context *ctx)
{
    HMAC_CTX c;
    unsigned char hmac[SHA1_DIGEST_LENGTH];
    unsigned int hmac_len;
    dtp_pkt *packet;
    int flags = DTP_FLAG_RSP;
    int hdr_size;
    struct msghdr mh;
    struct iovec iov[2];

    /* encrypt with nonce from the payload */
    /* hmac = HMAC_key(IP_a, IP_b, nonce) */
    HMAC_CTX_init(&c);

    /* old openssl library */
#if OPENSSL_VERSION_NUMBER <= 0x009080ffL
    HMAC_Init_ex(&c, ctx->tc_key, sizeof(ctx->tc_key), EVP_sha1(), NULL);
    HMAC_Update(&c, (const unsigned char*)&(ctx->tc_nonce), 
				sizeof(ctx->tc_nonce));
    HMAC_Final(&c, hmac, &hmac_len);
    /* new openssl library */
#else
    if (HMAC_Init_ex(&c, ctx->tc_key, sizeof(ctx->tc_key), 
					 EVP_sha1(), NULL) == 0) {
        TRACE("HMAC_Init_ex failed\n");
		EXIT(-1, return);
    }
    if (HMAC_Update(&c, (const unsigned char*)&(ctx->tc_nonce), 
					sizeof(ctx->tc_nonce)) == 0) {
        TRACE("HMAC_Update failed\n");
        EXIT(-1, return);
    }
    if (HMAC_Final(&c, hmac, &hmac_len) == 0) {
        TRACE("HMAC_Final failed\n");
        EXIT(-1, return);
    }
#endif

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

    /* get idle packet */
    packet = DTPGetIdlePacket();
    ASSERT(packet, return);

    /* generate header */
    hdr_size = DTPGenerateHeader(packet->tp_header, 0, 0, 
								 ctx->tc_flowID, flags,
								 (GetAvailReadBufSize(ctx) >>
								  ctx->tc_recvWindowScale));
	ASSERT(hdr_size > 0, return);
    hdr_size = ((hdr_size + 3) & ~0x3);

    /* generate packet */
    packet->tp_ctx = ctx;
    DTPInsertPacket(packet, hdr_size + SHA1_DIGEST_LENGTH, hdr_size);
    DTPGeneratePacket(&mh, iov, packet, hmac);
    
    /* send RSP packet with nonce */
    TRACE("about to send RSP packet\n");
#ifdef DEBUGX
    DTPPrintPacket(packet->tp_header, packet->tp_len);
#endif
    
    DTPSendPacket(ctx->tc_sock, &mh, hdr_size + SHA1_DIGEST_LENGTH,
				  SENDMSG, &ctx->tc_peerAddr, sizeof(struct sockaddr_in), 
				  ctx);

    /* remove packet */
    AddToFreePacketList(packet);
}
/*-------------------------------------------------------------------*/
static void
SendAUTHPacket(dtp_context *ctx)
{
    u_char hdr[DTPHDRSIZE];
    socklen_t addrlen = sizeof(ctx->tc_peerAddr);
    int flags = DTP_FLAG_AUTH | DTP_FLAG_ACK;
    int hdr_size;

#ifdef IN_MOBILE
	if (IsMobileConnected())
		flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

    /* generate header */
    hdr_size = DTPGenerateHeader(hdr, ctx->tc_lastByteSent, 
								 GetAckNumber(ctx), 
								 ctx->tc_flowID, flags, 
								 (GetAvailReadBufSize(ctx) >> 
								  ctx->tc_recvWindowScale));
  
    hdr_size = ((hdr_size + 3) & ~0x3);

    TRACE("about to send an AUTH\n");
#ifdef DEBUGX
    DTPPrintPacket(hdr, hdr_size);
#endif

    /* send packet */
    DTPSendPacket(ctx->tc_sock, hdr, hdr_size, SENDTO,
				  &ctx->tc_peerAddr, addrlen, NULL);

    /* set AUTH sent */
    ctx->tc_isAUTHSent = TRUE;
}
/*-------------------------------------------------------------------*/
static void
HandleAddressChange(dtp_context *ctx, u_char *buf, struct sockaddr_in* addr)
{
	if (ctx->tc_doingCHGRSP == 0)
		ctx->tc_doingCHGRSP = 1;
	/* send CHG packet */
	SendCHGPacket(ctx, addr);
}
/*-------------------------------------------------------------------*/
static void
HandleCHGPacket(dtp_context *ctx, u_char *buf, struct sockaddr_in *addr)
{

	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- CHG   \tseq=%08X\tfid=%08X",
			ctx->tc_sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
    /* reset if other host's address has been also changed */
    if (memcmp(&ctx->tc_peerAddr, addr, 
			   sizeof(ctx->tc_peerAddr)) != 0) {
		TRACE("other host's address has been also changed");
		SendRSTPacket(ctx->tc_sock, buf, addr);
	
		/* socket's pending error is set to ECONNRESET */
		ctx->tc_connReset = TRUE;
		
		/* socket connection closed */
		ctx->tc_state = SOCK_CLOSED;
		
		/* let the socket function returns */
		DTPReadBufWakeup(ctx);
		DTPWriteBufWakeup(ctx);

		/* dhkim: Known bug: 
		 * Core was generated by `./DTPFlowGenerator'.
		 * Program terminated with signal 11, Segmentation fault.
		 * #0  RemoveFromRetransQueue (ctx=0x2aaaf27fa710) at /home/dtn/dprox_tester/libdtp/src/dtp_retrans_queue.c:62
		 * 62		TAILQ_REMOVE(&g_retTimerQHead, ctx, tc_link);
		 * (gdb) bt
		 * #0  RemoveFromRetransQueue (ctx=0x2aaaf27fa710) at /home/dtn/dprox_tester/libdtp/src/dtp_retrans_queue.c:62
		 * #1  0x00000000004125bb in HandleCHGPacket (addr=0x2aaaf3028bb0, buf=0x2aaaf3028d60 "e\313P\210", ctx=0x2aaaf27fa710)
		 *     at /home/dtn/dprox_tester/libdtp/src/dtp_transport.c:1562
		 * #2  OnReadEvent (sock=32, isListen=0x0) at /home/dtn/dprox_tester/libdtp/src/dtp_transport.c:3103
		 * #3  0x00002aaaaaefc94c in event_base_loop () from /usr/lib/libevent-2.0.so.5
		 * #4  0x00000000004141e0 in DTPLibThreadMain (arg=<optimized out>) at /home/dtn/dprox_tester/libdtp/src/dtp_transport.c:3347
		 * #5  0x00002aaaaacd7e9a in start_thread (arg=0x2aaaf302c700) at pthread_create.c:308
		 * #6  0x00002aaaab6023fd in clone () at ../sysdeps/unix/sysv/linux/x86_64/clone.S:112
		 * #7  0x0000000000000000 in ?? ()
		 *
		 * But there is no ip address change in test environment so
		 * Sending or receiving CHG packet is really weird 
		 */
		/* remove from retransmission queue */
		RemoveFromRetransQueue(ctx);
    }

    /* store nonce */
    memcpy(&ctx->tc_nonce, buf + GetHeaderLen(buf), sizeof(ctx->tc_nonce));

    /* send RSP packet with HMAC */
    SendRSPPacket(ctx);

}
/*-------------------------------------------------------------------*/
static void
HandleRSPPacket(dtp_context *ctx, u_char *buf, struct sockaddr_in *addr)
{
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- RSP   \tseq=%08X\tfid=%08X",
			ctx->tc_sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
    HMAC_CTX c;
    unsigned char hmac_recv[SHA1_DIGEST_LENGTH];
    unsigned char hmac[SHA1_DIGEST_LENGTH];
    unsigned int hmac_len;
	struct timeval tv;
    /* get HMAC */
    HMAC_CTX_init(&c);
    memcpy(hmac_recv, buf + GetHeaderLen(buf), SHA1_DIGEST_LENGTH);

    /* old openssl library */
#if OPENSSL_VERSION_NUMBER <= 0x009080ffL
    HMAC_Init_ex(&c, ctx->tc_key, sizeof(ctx->tc_key), EVP_sha1(), NULL);
    HMAC_Update(&c, (const unsigned char*)&(ctx->tc_nonce), 
				sizeof(ctx->tc_nonce));
    HMAC_Final(&c, hmac, &hmac_len);
    /* new openssl library */
#else
    if (HMAC_Init_ex(&c, ctx->tc_key, sizeof(ctx->tc_key), 
					 EVP_sha1(), NULL) == 0) {
        TRACE("HMAC_Init_ex failed\n");
		EXIT(-1, return);
    }
    if (HMAC_Update(&c, (const unsigned char*)&(ctx->tc_nonce), 
					sizeof(ctx->tc_nonce)) == 0) {
        TRACE("HMAC_Update failed\n");
        EXIT(-1, return);
    }
    if (HMAC_Final(&c, hmac, &hmac_len) == 0) {
        TRACE("HMAC_Final failed\n");
        EXIT(-1, return);
    }
#endif

    /* send RST if validation fails */
    if (memcmp(hmac_recv, hmac, SHA1_DIGEST_LENGTH) != 0) {
		TRACE("validation failed\n");
		if (ctx->tc_doingCHGRSP == 1)
			SendRSTPacket(ctx->tc_sock, buf, addr);
		return;
    }

	ctx->tc_doingCHGRSP = 0;

    /* set to changed address */
    TRACE("validation success\n");
    memcpy(&ctx->tc_peerAddr, addr, sizeof(ctx->tc_peerAddr));

    /* send AUTH packet */
    SendAUTHPacket(ctx);

	/* calculate duration time */
	gettimeofday(&tv, NULL);
    if (pthread_mutex_lock(&ctx->tc_connTimeLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
	if (ctx->tc_isClientMobile) {
		ctx->tc_mobileTime = (tv.tv_sec * 1000 + tv.tv_usec / 1000) - 
			(ctx->tc_startTime.tv_sec * 1000 + ctx->tc_startTime.tv_usec / 1000);
		ctx->tc_wifiTime = 0;
	}
	else {
		ctx->tc_mobileTime = 0;
		ctx->tc_wifiTime = (tv.tv_sec * 1000 + tv.tv_usec / 1000) - 
			(ctx->tc_startTime.tv_sec * 1000 + ctx->tc_startTime.tv_usec / 1000);
	}
    if (pthread_mutex_unlock(&ctx->tc_connTimeLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }

	/* check whether client is in mobile network */
	gettimeofday(&ctx->tc_startTime, NULL);
	if (IsMobilePacket(buf))
		ctx->tc_isClientMobile = TRUE;
	else
		ctx->tc_isClientMobile = FALSE;

    /* resume data transfer */
    if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
		perror("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
	/* DOUBLE CHECK : added after merging with scheduler */
	DTPSlowStart(ctx);
    DTPWriteAvail(ctx->tc_sock, ctx);
	DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
    if (ctx->tc_isWriteBlocked) {
		if (pthread_cond_signal(&ctx->tc_writeBufCond)) {
			perror("pthread_cond_signal() failed");
			EXIT(-1, );
		}
    }    
    if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
		perror("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }

	/* retransmit the packet in the queue */
    dtp_pkt *packet = NULL;
    if ((packet = TAILQ_FIRST(&ctx->tc_packetQHead)) != NULL) {
		DTPRetransmit(ctx->tc_sock, packet);
	}
}
/*-------------------------------------------------------------------*/
static void
HandleAUTHPacket(dtp_context *ctx, u_char *buf, struct sockaddr_in *addr)
{
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- AUTH  \tseq=%08X\tfid=%08X",
			ctx->tc_sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
    dtp_pkt *packet = NULL;

    /* check whether other host's address has been changed */
    if (memcmp(&ctx->tc_peerAddr, addr, 
			   sizeof(ctx->tc_peerAddr)) != 0) {
		HandleAddressChange(ctx, buf, addr);
		return;
    }

    /* remove ACK packet in packet list */
    if (ctx->tc_isACKInserted && !TAILQ_EMPTY(&ctx->tc_packetQHead)) {
    	packet = TAILQ_FIRST(&ctx->tc_packetQHead);
    	if (!IsACKPacket((u_char *)packet))
			return;
    	TAILQ_REMOVE(&ctx->tc_packetQHead, packet, tp_link);
    	AddToFreePacketList(packet);
    	ctx->tc_isACKInserted = FALSE;
    }

    /* resume data transfer */
    if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
		perror("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
	/* DOUBLE CHECK : added after merging with scheduler */
	/* FIX: Multiple arrival of AUTH can occur SlowStart twice
	 (rev. 885) */
	DTPSlowStart(ctx); /* slow start after CHG-RSP-AUTH */
    DTPWriteAvail(ctx->tc_sock, ctx);
	DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
    if (ctx->tc_isWriteBlocked) {
		if (pthread_cond_signal(&ctx->tc_writeBufCond)) {
			perror("pthread_cond_signal() failed");
			EXIT(-1, );
		}
    }    
    if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
		perror("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }

	/* DOUBLE CHECK : added after merging with scheduler */
	DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");

	DTPSendACKPacket(ctx);	/* send ACK packet to resume the transmission */
}
/*-------------------------------------------------------------------*/
void
HandleDisconnect(dtp_context *ctx)
{
    dtp_pkt *packet, *pdone = NULL;
    int firstPacket = TRUE;
    int hasPacket = FALSE;
    u_char hdr[DTPHDRSIZE];
    int hdr_size;

    TRACE("network is disconnected\n");
    ctx->tc_isNetConnected = FALSE;
    
    /* remove from the packet list except first packet */
    TAILQ_FOREACH(packet, &ctx->tc_packetQHead, tp_link) {
		hasPacket = TRUE;
		if (pdone) {
			TAILQ_REMOVE(&ctx->tc_packetQHead, pdone, tp_link);
			AddToFreePacketList(pdone);
			pdone = NULL;
		}
		if (firstPacket) {
			TRACE("probe packet!\n");
#ifdef DEBUGX
			DTPPrintPacket(packet->tp_header, packet->tp_len);
#endif
			ctx->tc_lastByteSent = packet->tp_seqNum + 
				packet->tp_len - packet->tp_headerLen;
			firstPacket = FALSE;
		}
		else 
			pdone = packet;
    }
    if (pdone) {
		TAILQ_REMOVE(&ctx->tc_packetQHead, pdone, tp_link);
		AddToFreePacketList(pdone);
    }

    if (!hasPacket) {
		/* add ACK packet to packet queue */
		int flags = DTP_FLAG_ACK;
		ctx->tc_isACKInserted = TRUE;

#ifdef IN_MOBILE
		if (IsMobileConnected())
			flags |= DTP_FLAG_MOBILE; /* need to have MOBILE */
#endif

		hdr_size = DTPGenerateHeader(hdr, ctx->tc_lastByteSent, 
									 GetAckNumber(ctx),
									 ctx->tc_flowID, flags, 
									 (GetAvailReadBufSize(ctx) >> 
									  ctx->tc_recvWindowScale));
		hdr_size = ((hdr_size + 3) & ~0x3);

		packet = DTPGetIdlePacket();
		ASSERT(packet, return);
		memcpy(packet->tp_header, hdr, hdr_size); 
		packet->tp_ctx = ctx;

		DTPInsertPacket(packet, hdr_size, hdr_size);

		TAILQ_INSERT_TAIL(&ctx->tc_packetQHead, packet, tp_link);
    }

    /* set timeout to 1 sec */
    ctx->tc_RTO = 1;

}
/*-------------------------------------------------------------------*/
static void
HandleFINPacket(dtp_context *ctx, u_char* buf, int len)
{
	DHK_TFLOG(DHK_DEBUG & (DPKT | DFINPKT), DHK_F_BASE"/packet.txt",
			"fd=%d\t<- FIN   \tseq=%08X\tfid=%08X",
			ctx->tc_sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
    uint32_t seqnum = ntohl(GetSeqNum(buf));
    uint32_t ack_num = ntohl(GetAckNum(buf));
    dtp_pkt *packet;

	/* Ignore FIN before ESTABLISHED */
	if ((ctx->tc_state == SOCK_CLOSED
				|| ctx->tc_state == SOCK_LISTEN
				|| ctx->tc_state == SOCK_SYN_RCVD
				|| ctx->tc_state == SOCK_SYN_SENT))
		return;
    /* don't handle FIN if sent packets are not ACKed yet */
    /* use lastByteWritten since lastByteSent decreases at timeout */
    if ((ctx->tc_state == SOCK_FIN_WAIT_1 || 
		 ctx->tc_state == SOCK_FIN_WAIT_2) && 
		(ctx->tc_lastByteWritten != ack_num)) {
        if (ack_num != ctx->tc_lastByteWritten + 1 &&
            !(ack_num == ctx->tc_lastByteWritten - 1 && ctx->tc_isFINSent)) {
			return;
		}
		ASSERT(ctx->tc_closeCalled, );
		ASSERT(seqnum == ctx->tc_nextByteExpected, );
    }
    else if ((ctx->tc_state == SOCK_ESTABLISHED) && 
			 (seqnum != ctx->tc_nextByteExpected)) {
		return;
    }

    /* send ACK */
	DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
    DTPSendACKPacket(ctx);

    /* acquire a lock to change the state */
    if (pthread_mutex_lock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
   
    ctx->tc_isFINRcvd = TRUE;

    // [5-1] when recved FIN on when empty read buf, return select.
	DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ);

    /* change context state */
    if ((ctx->tc_state == SOCK_ESTABLISHED) ||
		(ctx->tc_state == SOCK_CLOSE_WAIT)) {
		ctx->tc_state = SOCK_CLOSE_WAIT;
    }

    if (pthread_mutex_unlock(&ctx->tc_readBufLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }

    if (ctx->tc_state == SOCK_CLOSE_WAIT) {
		/* alert that FIN is received */
		DHK_FDBG(DHK_DEBUG & DTEMP, DHK_F_BASE"temp.txt",
				"fid=%08X: state = SOCK_CLOSE_WAIT. Wakeup buffers.",
				ntohl(GetFlowID(buf)));
		DTPReadBufWakeup(ctx);
		DTPWriteBufWakeup(ctx);
		/* 131111 dhkim: FIXME: Ugly HOTFIX
		 * Forcing send FIN is not a good idea.
		 * In most cases, FIN will be sended twice (Once from here, and another
		 * from original FIN handling procedure).
		 * However, This removes all dtp_close()-ed but not actually close()-ed
		 * sockets in server(who receives FIN and resends FIN) and make client
		 * side(who sends FIN first) opened FDs will not grow anymore after
		 * 30 seconds*/
//		DTPWriteAvail(ctx->tc_sock, ctx);
//		DTPSendFINPacket(ctx);
    }
    else if ((ctx->tc_state == SOCK_FIN_WAIT_1) ||
			 (ctx->tc_state == SOCK_FIN_WAIT_2)) {
		/* remove FIN packet */
		if (ctx->tc_state == SOCK_FIN_WAIT_1) {
			packet = TAILQ_FIRST(&ctx->tc_packetQHead);
			TAILQ_REMOVE(&ctx->tc_packetQHead, packet, tp_link);
			AddToFreePacketList(packet);
		}

		/* enter 30 sec TIME_WAIT */
		ctx->tc_state = SOCK_TIME_WAIT;
		if (gettimeofday(&ctx->tc_waitTime, NULL)) {
			perror("gettimeofday() failed");
			EXIT(-1, );
		}
		DTPCloseContext(ctx);
    }
}
/*-------------------------------------------------------------------*/
static void
HandleDuplicateACK(dtp_context *ctx, uint32_t ack_num)
{
    dtp_pkt *packet;

    /* fast retransmit */
    TRACE("3 dup-ACKs received. doing a fast-retransmit\n");
    
    /* ACK for already removed packet */
    if ((packet = TAILQ_FIRST(&ctx->tc_packetQHead)) == NULL)
		return;
    
    ASSERT(packet->tp_seqNum <= ack_num, );
    ASSERT(ack_num <= packet->tp_seqNum + 
		   (packet->tp_len - packet->tp_headerLen), );
    
    DTPRetransmit(ctx->tc_sock, packet);
    
    /* enter congestion avoidance state */
    CongestionAvoidance(ctx);
}
/*-------------------------------------------------------------------*/
static void
RemovePacket(dtp_context *ctx, uint32_t ack_num)
{
    dtp_pkt *packet, *pdone = NULL;
    
    TAILQ_FOREACH(packet, &ctx->tc_packetQHead, tp_link) {
		if (pdone) {
			if (!pdone->tp_isRetrans) {
				ctx->tc_RTO = CalculateRTO(pdone);
				/* don't go below minimum RTO */
				if (ctx->tc_RTO < RTO_MIN)
					ctx->tc_RTO = RTO_MIN;
			}
			ctx->tc_segSent -= (pdone->tp_len - pdone->tp_headerLen);

			TAILQ_REMOVE(&ctx->tc_packetQHead, pdone, tp_link);
			AddToFreePacketList(pdone);
			pdone = NULL;
	    
			ctx->tc_isCongested = FALSE;
			ctx->tc_numACKRcvd = 1;
		}

		/* Check the position of the ack_num in packetQ list */
		/* 1) packet seq + len < ACK */
		if ((packet->tp_seqNum + packet->tp_len -
			 packet->tp_headerLen) < ack_num) {
			/* remember to free packet */
			pdone = packet;
		}
		/* 2) packet seq + len == ACK */
		else if ((packet->tp_seqNum + packet->tp_len -
				  packet->tp_headerLen) == ack_num) {
			/* remember to free packet if it's not FIN */
			if (!((struct dtp_hdr *)packet->tp_header)->fin) 
				pdone = packet;
			break;
		}
		/* 3) ACK < packet seq + len */
		else {
			int move_len = ack_num - packet->tp_seqNum;
			packet->tp_seqNum += move_len;
			/* update the seq number in header field */	    
			((struct dtp_hdr *)(packet->tp_header))->seq
				= htonl(packet->tp_seqNum);
			packet->tp_len -= move_len;
			ctx->tc_segSent -= move_len;
			break;
		}

    }
    if (pdone) {
		if (!pdone->tp_isRetrans) {
			ctx->tc_RTO = CalculateRTO(pdone);
			/* don't go below minimum RTO */
			if (ctx->tc_RTO < RTO_MIN)
				ctx->tc_RTO = RTO_MIN;
		}
		ctx->tc_segSent -= (pdone->tp_len - pdone->tp_headerLen);
		TAILQ_REMOVE(&ctx->tc_packetQHead, pdone, tp_link);
		AddToFreePacketList(pdone);
		pdone = NULL;

		ctx->tc_isCongested = FALSE;
		ctx->tc_numACKRcvd = 1;
    }
}
/*-------------------------------------------------------------------*/
static void
HandleACKPacket(dtp_context *ctx, uint32_t ack_num, uint16_t window_size)
{
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- ACK   \tack=%08X\tfid=%08X",
			ctx->tc_sock, ack_num, ctx->tc_flowID);
    int acked;

    /* return at conditions:
     * 1) ignore ACKs in queue when disconnected
     * 2) ignore old ACK
     * 3) ignore ACK in TIME_WAIT state
     */
	
    if (!ctx->tc_isNetConnected) {
		return;
	}

	/* check tcp wrap-around */
	if (ack_num < ctx->tc_lastByteAcked) {
		if (ctx->tc_lastByteAcked + DTP_MTU < ctx->tc_lastByteAcked) {
			ctx->tc_nextByteExpected++;
		}
		else
			return;
	}

    /* acquire a lock to access the shared variables */
    if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
		perror("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }

    /* check whether the other host's window size 0 */
    if ((ctx->tc_rcvWindow = (window_size << ctx->tc_sendWindowScale)) == 0) {
		ctx->tc_isHerWindowZero = TRUE;

		/* persist timer : start timer and set initial probe interval */
		if (gettimeofday(&ctx->tc_persistTimer, NULL)) {
			perror("gettimeofday() failed");
			EXIT(-1, );
		}
		ctx->tc_persistIntvl = DTP_PERSIST_MIN;		
    }

    /* recover from window size = 0 */
    if (ctx->tc_isHerWindowZero && (ctx->tc_rcvWindow > 0)) {
		ctx->tc_isHerWindowZero = FALSE;

		/* write to destination */
		DTPWriteAvail(ctx->tc_sock, ctx);	  
		DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);

		if (ctx->tc_isWriteBlocked) {
			if (pthread_cond_signal(&ctx->tc_writeBufCond)) {
				perror("pthread_cond_signal() failed");
				EXIT(-1, );
			}
		}    
		if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
			perror("pthread_mutex_unlock() failed");
			EXIT(-1, );
		}
		return;
    }	

    /* duplicate ACK */
    if (ack_num == ctx->tc_lastByteAcked) {
		/* 3 duplicate ACK (which means 4 identical ACKs) */
		if (++ctx->tc_numACKRcvd == 4) {
			HandleDuplicateACK(ctx, ack_num);
	    
			if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
				perror("pthread_mutex_unlock() failed");
				EXIT(-1, );
			}
			return;
		}
    }

	acked = ack_num - ctx->tc_lastByteAcked;
    ctx->tc_lastByteAcked = ack_num;

	/* increment lastByteSent for packet sent before disconnection */
    if (ctx->tc_lastByteSent < ctx->tc_lastByteAcked)
		ctx->tc_lastByteSent = ctx->tc_lastByteAcked;

    if (GetWriteBufOff(ctx) > 0) {
		memmove(ctx->tc_writeBuf, 
				ctx->tc_writeBuf + acked, 
				GetWriteBufOff(ctx));
    }

    /* remove ACKed packets */
    RemovePacket(ctx, ack_num);

    if (acked) {
		/* increase uplink byte count by acked size */
		if (pthread_mutex_lock(&ctx->tc_upByteLock)) {
			TRACE_ERR("pthread_mutex_lock() failed");
			EXIT(-1, return);
		}
		ctx->tc_upByte += acked;
		if (pthread_mutex_unlock(&ctx->tc_upByteLock)) {
			TRACE_ERR("pthread_mutex_unlock() failed");
			EXIT(-1, );
		}

		if (ctx->tc_state == SOCK_SYN_RCVD) {
			ctx->tc_state = SOCK_ESTABLISHED;
			// [22] SYN_RCVD => ESTABLISHED
			if ((ctx->tc_state == SOCK_CLOSE_WAIT
						|| ctx->tc_state == SOCK_LAST_ACK
						|| ctx->tc_state == SOCK_CLOSED
						|| ctx->tc_state == SOCK_FIN_WAIT_1
						|| ctx->tc_state == SOCK_FIN_WAIT_2
						|| ctx->tc_state == SOCK_TIME_WAIT))
				DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
						"fid=%08X: Trying to DTPSelectEventClr() in SOCK_CLOSE_WAIT state",
						ctx->tc_flowID);
			DTPSelectEventClr(ctx->tc_sock, DTP_FD_READ);
			DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
		}
		MoveCongestionWindow(ctx, acked);

		// [6] empty space after ACK
		DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
    }

    DTPWriteAvail(ctx->tc_sock, ctx);
	DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
    if (ctx->tc_isWriteBlocked) {
		if (pthread_cond_signal(&ctx->tc_writeBufCond)) {
			perror("pthread_cond_signal() failed");
			EXIT(-1, );
		}
    }    
    if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
		perror("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }
    
    /* receive ACK for sent FIN */
    if ((ctx->tc_state == SOCK_FIN_WAIT_1) && 
		(ctx->tc_lastByteWritten == ack_num)) {
		ctx->tc_state = SOCK_FIN_WAIT_2;
    }
    /* receive last ACK */
    else if (ctx->tc_state == SOCK_LAST_ACK) {
		ctx->tc_state = SOCK_CLOSED;
		DTPReadBufWakeup(ctx);
		/* remove context from g_retTimerQHead queue */
		RemoveFromRetransQueue(ctx);

		DTPCloseContext(ctx);
    }
}
/*-------------------------------------------------------------------*/
/* return the context pointer on success, NULL on failure (full
   listenQ or invalid SYN) */
static dtp_context *
HandleSYNPacket(int sock, u_char *buf, struct sockaddr_in* addr)
{
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- SYN   \tseq=%08X\tfid=%08X",
			sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
    /* 
       1. a new connection
       2. an old connection that is reconnected
       3. flowID conflict with another flow
    */
    dtp_context* listen_ctx;
    dtp_context* ctx;
    uint32_t seqnum, fid;
    u_char hid[SHA1_DIGEST_LENGTH];
    uint32_t deadline = 0;
    struct sockaddr_in new_address;
    int new_addressLen = sizeof(new_address);
    dtp_event ev;
    int res;

    /* get flow ID, host ID, deadline */
    fid = GetFlowID(buf);
    fid = ntohl(fid);

    if (!DTPGetOption(buf, hid, &deadline)) {
		TRACE("DTPGetOption() error\n");
		SendRSTPacket(sock, buf, addr);
		return(NULL);
    }
    
    /* check whether flow ID already exists */
    if ((ctx = DTPGetContextByFlowID(fid)) != NULL) {
		/* old connection */
		if (memcmp(ctx->tc_hostID, hid, SHA1_DIGEST_LENGTH) == 0) {
			TRACE("old connection is reconnected\n");
			ctx->tc_peerAddr = *addr;
			
			/* resend SYN+ACK packet */
			DTPHandleKey(sock, ctx, buf, 1);
			return(NULL); // fixed on 2013.09.20 (retransmitted SYN w/ different addr)
		}
		else {
			/* flow ID conflict */
			TRACE("flow ID conflict\n");
			SendRSTPacket(sock, buf, addr);

			return(NULL);
		}
    }

    /* error checking */
    listen_ctx = DTPGetContextBySocket(sock);
    if (listen_ctx == NULL || 
		(listen_ctx->tc_qidx == listen_ctx->tc_backlog)) {
		TRACE("error: invalid socket descriptor or listenQ is filled up\n");
		SendRSTPacket(sock, buf, addr);
		return(NULL);
    }

    /* --------------------------------- */

    ctx = DTPCreateContext();
	if (ctx == NULL) {
		return (NULL);
	}

    ctx->tc_flowID   = fid;
    ctx->tc_peerAddr = *addr;
    ctx->tc_state    = SOCK_SYN_RCVD;
    DTPAddContextToFlowIDMap(ctx->tc_flowID, ctx);
    memcpy(ctx->tc_hostID, hid, SHA1_DIGEST_LENGTH);

    seqnum = ntohl(GetSeqNum(buf));

	/* dhkim: FIXME: just for debugging */
	ctx->tc_lastByteSent = 0;
	//	ctx->tc_lastByteSent = rand();
	ctx->tc_lastByteAcked = ctx->tc_lastByteSent;
    ctx->tc_nextByteExpected = seqnum;
    ctx->tc_lastByteRcvd = seqnum;
    ctx->tc_lastByteRead = seqnum;

	/* check whether client is in mobile network */
	gettimeofday(&ctx->tc_startTime, NULL);
	if (IsMobilePacket(buf))
		ctx->tc_isClientMobile = TRUE;
	else 
		ctx->tc_isClientMobile = FALSE;
	ctx->tc_mobileTime = 0;
	ctx->tc_wifiTime = 0;

	/* deadline */
    struct timeval tv;
    ctx->tc_deadline = deadline;
	gettimeofday(&tv, NULL);
	if (ctx->tc_deadline > 0) {
		ctx->tc_deadlineTime.tv_sec = tv.tv_sec + ctx->tc_deadline;
		ctx->tc_deadlineTime.tv_usec = 0;
	}
	ctx->tc_isDeadlineSet = TRUE;
	//XXX
	TRACE("deadline = %d\n", ctx->tc_deadline);

    DTPParseOption(buf, (void *)ctx, 1);

    /* add the connection to the listenQ */
    if (pthread_mutex_lock(&listen_ctx->tc_readBufLock)) {
		TRACE("mutex lock on listenQ failed\n");
		return (NULL);
    }
    
    listen_ctx->tc_listenQ[listen_ctx->tc_qidx++] = ctx->tc_sock;

	DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ);
    if (listen_ctx->tc_isReadBlocked) {
		listen_ctx->tc_isTransportWaiting = TRUE;
		if (pthread_cond_signal(&listen_ctx->tc_readBufCond)) {
			TRACE("pthread_cond_signal failed\n");
			return (NULL);
		}
		if (pthread_cond_wait(&listen_ctx->tc_readBufCond, 
							  &listen_ctx->tc_readBufLock)) {
			TRACE_ERR("pthread_cond_wait() failed");
			return (NULL);
		}
		listen_ctx->tc_isTransportWaiting = FALSE;	    
    }

    /* -------------------- */

    /* bind new physical socket to a random port number */
    getsockname(ctx->tc_sock, (struct sockaddr*)&new_address,
				(socklen_t*)&new_addressLen);
    int i = 49152;/*1025;*/
    while (1) {
		if (++i == 65535)
			i = 49153;/*1026;*/
		new_address.sin_port = htons(i);
		res = bind(ctx->tc_sock, (struct sockaddr*)&new_address, new_addressLen);
		if (res == 0) {
			ctx->tc_isBound = TRUE;
			break;
		}
    }

    /* register this socket for data communication */
    if (!ctx->tc_beingMonitored) {
		ctx->tc_beingMonitored = true;
		ev.te_fd = ctx->tc_sock;
		ev.te_command = DTP_ADD_LISTEN_EVENT;
		DTPSendEventToLibThread(listen_ctx->tc_isock, &ev);
    }

    /* --------------------------- */

    if (pthread_mutex_unlock(&listen_ctx->tc_readBufLock)) {
		TRACE("mutex unlock on listenQ failed\n");
		EXIT(-1, );
    }

    /* get public key + generate symmetric key + send */
    DTPHandleKey(sock, ctx, buf, 0);

	/* a new connection in listen socket, wake up select() */
	// [33] new SYN arrived at listen packet- accept() can be done
	DTPSelectEventSet(sock, DTP_FD_READ);

    return(ctx);
}
/*-------------------------------------------------------------------*/
static dtp_context *
HandleSYNACKPacket(int sock, u_char *buf, struct sockaddr_in* addr)
{
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- SYNACK\tseq=%08X\tfid=%08X",
			sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
    dtp_context* ctx;
    u_char hid[SHA1_DIGEST_LENGTH];
    uint32_t deadline = 0;
    u_char encrypt[RSA_LEN];
    uint32_t seqnum = ntohl(GetSeqNum(buf));
    int res;

    ctx = DTPGetContextBySocket(sock);

    if (ctx->tc_state == SOCK_SYN_SENT) {
		/* get other host's ID & agree on deadline */
		if (!DTPGetOption(buf, hid, &deadline)) {
			TRACE("DTPGetOption() error\n");
			SendRSTPacket(sock, buf, addr);
			DHK_FDBG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
					"fd=%d, ctx=0x%08lX, state=%d",
					ctx->tc_sock, (unsigned long)ctx, ctx->tc_state);
			return(NULL);
		}
		memcpy(ctx->tc_hostID, hid, SHA1_DIGEST_LENGTH);

		//XXX
		TRACE("deadline = %d\n", ctx->tc_deadline);
		// TO BE FIXED? : check if the respond matches
		// ctx->tc_deadline = deadline;
	
		/* get HMAC key */
		memcpy(encrypt, buf + GetHeaderLen(buf), RSA_LEN);

		res = RSA_private_decrypt(RSA_LEN, encrypt, ctx->tc_key, 
								  g_rsa, RSA_PKCS1_OAEP_PADDING);
		if (res == -1) {
			TRACE("RSA_private_decrypt error (%s)\n", 
				  ERR_error_string((unsigned long)ERR_get_error(), NULL));
			return NULL;
		} 
	
		/* set offset */
		ctx->tc_state = SOCK_ESTABLISHED;
		ctx->tc_nextByteExpected = seqnum + RSA_LEN;
		ctx->tc_lastByteRcvd = seqnum + RSA_LEN;
		ctx->tc_lastByteRead = seqnum + RSA_LEN;

		ctx->tc_peerAddr = *addr;

		ctx = DTPGetContextBySocket(sock);
		DTPParseOption(buf, (void *)ctx, 1);

		// [11] SYN_SENT => ESTABLISHED
		ctx = DTPGetContextBySocket(sock);
		if ((ctx->tc_state == SOCK_CLOSE_WAIT
					|| ctx->tc_state == SOCK_LAST_ACK
					|| ctx->tc_state == SOCK_CLOSED
					|| ctx->tc_state == SOCK_FIN_WAIT_1
					|| ctx->tc_state == SOCK_FIN_WAIT_2
					|| ctx->tc_state == SOCK_TIME_WAIT))
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
					"fid=%08X: Trying to DTPSelectEventClr() in SOCK_CLOSE_WAIT state",
					ctx->tc_flowID);
		DTPSelectEventClr(ctx->tc_sock, DTP_FD_READ);
    }

	ctx = DTPGetContextBySocket(sock);
	DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
    DTPSendACKPacket(ctx);

	// DTN : ADD TO SCHEDULING LIST AND 

    return(ctx);
}
/*-------------------------------------------------------------------*/
static void
HandleRSTPacket(dtp_context *ctx, u_char* buf)
{
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- RST   \tseq=%08X\tfid=%08X",
			ctx->tc_sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
    u_char hid[SHA1_DIGEST_LENGTH];
    dtp_pkt *packet;
    TRACE("RST arrived\n");

    /* get host ID */
    if (!DTPGetOption(buf, hid, 0)) {
		TRACE("DTPGetOption() error\n");
		return;
    }
    
    /* ignore RST packet if host ID differs */
    if (memcmp(ctx->tc_hostID, hid, SHA1_DIGEST_LENGTH) != 0) {
		TRACE("received RST packet from different host\n");
		return;
    }

    /* flow ID collision */
    if (ctx->tc_state == SOCK_SYN_SENT) {
		/* remove SYN packet from packet list */
		packet = TAILQ_FIRST(&ctx->tc_packetQHead);
		TAILQ_REMOVE(&ctx->tc_packetQHead, packet, tp_link);

		/* generate new flow ID */
		DTPGenerateHostID(hid);
		ctx->tc_flowID = DTPGenerateFlowID(hid);
	
		/* initialize */
		ctx->tc_lastByteSent -= packet->tp_len - packet->tp_headerLen;
		ctx->tc_segSent = 0;
		AddToFreePacketList(packet);
		DTPWriteAvail(ctx->tc_sock, ctx);
		return;
    }
    /* server failure */
    else if (ctx->tc_state == SOCK_ESTABLISHED) {
		/* socket's pending error is set to ECONNRESET */
		ctx->tc_connReset = TRUE;

		// [7] RST
		DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ);
		DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
		    
		/* socket connection closed */
		ctx->tc_state = SOCK_CLOSED;

		/* let the socket function returns */
		DTPReadBufWakeup(ctx);
		DTPWriteBufWakeup(ctx);

		/* remove from retransmission timer queue */
		RemoveFromRetransQueue(ctx);
    }
    /* client closed after SOCK_TIME_WAIT */
    else if (ctx->tc_state == SOCK_LAST_ACK) {
		/* close server context */
		packet = TAILQ_FIRST(&ctx->tc_packetQHead);
		TAILQ_REMOVE(&ctx->tc_packetQHead, packet, tp_link);
		packet->tp_ctx->tc_state = SOCK_CLOSED;
		AddToFreePacketList(packet);
    }
}
/*-------------------------------------------------------------------*/
static int
CalculateOverlap(uint32_t seq, uint32_t len, buffer *buf, int *overlap_len)
{
    uint32_t start, end;
    /* packet buffer is overlapped */
		/*(seq <= buf->seq && seq + len >= buf->seq) ||
        (seq <= buf->seq + buf->len && seq + len >= buf->seq + buf->len)*/
		/* calculate length of overlapped region */
	start = MAX(seq, buf->seq);
	end = MIN(seq + len, buf->seq + buf->len);
	*overlap_len = end - start;
	if (*overlap_len >= 0) {
        /* set new payload length and seq */
        buf->len = MAX(buf->seq + buf->len, seq + len)
            - MIN(buf->seq, seq);
        buf->seq = MIN(buf->seq, seq);

        return TRUE;
    }

    return FALSE;
}
/*-------------------------------------------------------------------*/
static int
HandleNormalPacket(dtp_context *ctx, u_char* buf, uint32_t bodyLen)
{
	DHK_TFLOG(DHK_DEBUG & DPKT, DHK_F_BASE"/packet.txt",
			"fd=%d\t<- Normal\tseq=%08X\tfid=%08X",
			ctx->tc_sock, ntohl(GetSeqNum(buf)), ntohl(GetFlowID(buf)));
	
	DHK_FDBG(DHK_DEBUG & DMISMATCH && ctx->tc_sock == 100,
			DHK_F_BASE"mismatch.txt",
			"sock = %d, seq = %d, len = %d",
			ctx->tc_sock, ntohl(GetSeqNum(buf)), bodyLen);

    dtp_pkt *packet;

    if (ctx->tc_state == SOCK_SYN_RCVD ||
		ctx->tc_state == SOCK_SYN_SENT ||
		IsSYNPacket(buf)) {
		return 0;
    }

    /* remove ACK packet in queue for download case */
    if (ctx->tc_isACKInserted && !TAILQ_EMPTY(&ctx->tc_packetQHead)) {
		packet = TAILQ_FIRST(&ctx->tc_packetQHead);
		if (IsACKPacket((u_char *)packet->tp_header)) {
			TAILQ_REMOVE(&ctx->tc_packetQHead, packet, tp_link);
			AddToFreePacketList(packet);
			ctx->tc_isACKInserted = FALSE;
		}
		else
			ASSERT(0, );
    }

    /* a normal packet = packet with non-zero payload */
    uint32_t seqnum = ntohl(GetSeqNum(buf));
    uint32_t window_size = ntohs(GetRcvdWindow(buf));
	int pkt_off = 0;   // packet offset
	int buf_off = 0;   // read buffer offset from nextByteExpected
    int writeLen = 0;

    ctx->tc_seqNum = seqnum;
    ctx->tc_rcvWindow = (window_size << ctx->tc_sendWindowScale);
    
    /* check whether window size is 0 */
    if (ctx->tc_rcvWindow == 0) {
		ctx->tc_isHerWindowZero = TRUE;
    }

    /* mark that we received some data */
    ctx->tc_recvAnyData = TRUE;

	/* calculate writeLen and buf_off */	
	uint32_t start = MAX(ctx->tc_nextByteExpected, seqnum);
	uint32_t end = __min(ctx->tc_lastByteRead + ctx->tc_readBufLen, seqnum + bodyLen);
	writeLen = MAX(0, end - start);

	if (writeLen > 0) {
		buf_off = start - ctx->tc_lastByteRead;
		pkt_off = start - seqnum;

		memcpy(ctx->tc_readBuf + buf_off,
			   buf + GetHeaderLen(buf) + pkt_off, writeLen);
	
		int overlap_len = 0, merged = FALSE;
		buffer *cur_buf, *temp_buf, *prev_buf, *pprev_buf;

		cur_buf = GetBuffer();

		cur_buf->seq = start;
		cur_buf->len = writeLen;	
		cur_buf->next_buf = NULL;

		/* search through the buffer list */	
		for (temp_buf = ctx->tc_readBufHead /**/, prev_buf = NULL, pprev_buf = NULL;
			 temp_buf != NULL;
			 pprev_buf = prev_buf, prev_buf = temp_buf, temp_buf = temp_buf->next_buf) {

			if (cur_buf->seq + cur_buf->len < temp_buf->seq)
				break;
		
			DHK_FLOG(DHK_DEBUG & DMISMATCH && ctx->tc_sock == 100,
					DHK_F_BASE"mismatch.txt",
					"Call CalculateOverlap. merge %d(%d) to %d(%d)",
					cur_buf->seq, cur_buf->len,
					temp_buf->seq, temp_buf->len);
			if (CalculateOverlap(cur_buf->seq, cur_buf->len, temp_buf, &overlap_len)) {
				DHK_FLOG(DHK_DEBUG & DMISMATCH && ctx->tc_sock == 100,
						DHK_F_BASE"mismatch.txt",
						"CalculateOverlap returned TRUE. merged %d(%d)."
						"OverlapLen was %d",
						temp_buf->seq, temp_buf->len, overlap_len);

				if (overlap_len > 0) {
					writeLen -= overlap_len;
				}
			
				/* remove buffer from buffer list if it's previous buffer */
				if (prev_buf == cur_buf) {
					if (pprev_buf) {
						/* connect previous previous buffer to current buffer */
						pprev_buf->next_buf = temp_buf;
					}
					else {
						/* set current buffer to head buffer */						
						ctx->tc_readBufHead = temp_buf;
					}
					prev_buf = pprev_buf;
				}
				AddToFreeBufferList(cur_buf);
				cur_buf = temp_buf;
				merged = TRUE;

			}
			else
				DHK_FLOG(DHK_DEBUG & DMISMATCH && ctx->tc_sock == 100,
						DHK_F_BASE"mismatch.txt",
						"CalculateOverlap returned FALSE.");
		} /* for (temp_buf) */

		/* no overlap */
		if (!merged) {

			/* add first buffer to the buffer ring */
			if (ctx->tc_readBufHead == NULL) {
				ctx->tc_readBufHead = cur_buf;
			}
			/* buffer has smaller seq than first buffer's seq in buffer list */
			else if (start < ctx->tc_readBufHead->seq + ctx->tc_readBufHead->len) {
				cur_buf->next_buf = ctx->tc_readBufHead;
				ctx->tc_readBufHead = cur_buf;
			}
			/* buffer has smaller seq than next buffer's seq in buffer list */
			else {
				if (prev_buf == NULL) {
					LOGD("POINT6");
					LOGD("start = %d, end = %d // readBufHead seq = %d, "
							"len = %d",
						 start, end, ctx->tc_readBufHead->seq,
						 ctx->tc_readBufHead->len);
					DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
							"sock %d, prev_buf == NULL: \n"
							"tc_readBufHead->seq=%08X, \n"
							"tc_readBufHead->len=%08x, \n"
							"start=%08X, writeLen=%08X",
							ctx->tc_sock,
							ctx->tc_readBufHead->seq, 
							ctx->tc_readBufHead->len,
							start, writeLen);
					if (temp_buf != NULL)
						DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
								"\ntemp_buf->seq=%08X,\n"
								"temp_buf->len=%08x",
								temp_buf->seq, temp_buf->len);
						
				}
				prev_buf->next_buf = cur_buf;
				cur_buf->next_buf = temp_buf;
			}
		}

		if (ctx->tc_readBufHead) {
			if (ctx->tc_nextByteExpected >= ctx->tc_readBufHead->seq &&
				ctx->tc_nextByteExpected < ctx->tc_readBufHead->seq + ctx->tc_readBufHead->len) {
				ctx->tc_nextByteExpected = ctx->tc_readBufHead->seq + ctx->tc_readBufHead->len;
			}
		}
		if (cur_buf->next_buf == NULL) {
			ctx->tc_lastByteRcvd = cur_buf->seq + cur_buf->len;
		}

		/* check if read buffer is full */
		DHK_FDBG(DHK_DEBUG & DRCVLOBUF & 0, DHK_F_BASE"window.txt",
				"%u >> %u, winsize zero %s",
				GetAvailReadBufSize(ctx), ctx->tc_recvWindowScale,
				ctx->tc_isMyWindowZero ? "TRUE" : "FALSE");
		if ((GetAvailReadBufSize(ctx) >> ctx->tc_recvWindowScale) == 0) {
			ctx->tc_isMyWindowZero = TRUE;
		}

		if (GetReadBufDataSize(ctx) > 0 && ctx->tc_lastByteRead != ctx->tc_readBufHead->seq) {
			LOGD("[sock %d, B][mismatch!] lBRead : %d / nBExpected : %d / readBufSeq : %d / headBuflen : %d",
				 ctx->tc_sock, ctx->tc_lastByteRead, ctx->tc_nextByteExpected, ctx->tc_readBufHead->seq, ctx->tc_readBufHead->len);			
			LOGD("%d %d", seqnum, bodyLen);
			DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",
					"[sock %d, B][mismatch!] lBRead : %d / "
					"nBExpected : %d / readBufSeq : %d / "
					"headBuflen : %d",
					ctx->tc_sock, ctx->tc_lastByteRead, 
					ctx->tc_nextByteExpected, ctx->tc_readBufHead->seq, 
					ctx->tc_readBufHead->len);
			EXIT(-1, );
		}

/*=============================================================================*/
    /* update the buffer and associated variables
			// increase downlink byte count by received size
			if (pthread_mutex_lock(&ctx->tc_downByteLock)) {
				TRACE_ERR("pthread_mutex_lock() failed");
				exit(-1);
			}
			ctx->tc_downByte += writeLen;
			if (pthread_mutex_unlock(&ctx->tc_downByteLock)) {
				TRACE_ERR("pthread_mutex_unlock() failed");
				exit(-1);
			}
	*/
/*=============================================================================*/
	}

    TRACE("got %d bytes, avail=%d, zerowindow=%d\n", 
		  writeLen, GetAvailReadBufSize(ctx), ctx->tc_isMyWindowZero);

    ctx->tc_numPacketRcvd++;

    /* send ACK if 2 packets are received */
    /*if (ctx->tc_numPacketRcvd == 2) {
	  TAILQ_REMOVE(&g_delayedAckQHead, ctx, tc_delayedAckLink);
	  ctx->tc_numPacketRcvd = 0;
	  DTPSendACKPacket(ctx);
	  }
	  else 
	  TAILQ_INSERT_TAIL(&g_delayedAckQHead, ctx, tc_delayedAckLink);*/

	//	LOGD("HandleNormalPacket end (%d)", writeLen);

    return writeLen;
}
/*-------------------------------------------------------------------*/
static void 
HandleTimeout(void)
{
    dtp_pkt *packet, *pdone = NULL;
    dtp_context *ctx, *pctx = NULL;
    struct timeval tv;
    double diff, now;
	static double prev = 0;
	bool printed = 0;

    if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, return);
    }
    now = tv.tv_sec + (tv.tv_usec / 1e6);
    if (pthread_mutex_lock(GetRetransQueueLock())) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
    /* packet queue */
    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
		if (pctx) {
			TAILQ_REMOVE(GetRetransQueue(), pctx, tc_link);
			pctx->tc_state = SOCK_CLOSED;
			DTPCloseContext(pctx);
			pctx = NULL;
		}

		/* 30 sec timeout for TIME_WAIT occurred */
		if (ctx->tc_state == SOCK_TIME_WAIT) {

			diff = now - ctx->tc_waitTime.tv_sec - (ctx->tc_waitTime.tv_usec / 1e6);
			/* check diff only if ctx->tc_waitTime has valid value */
			if (diff > LAST_TIMEOUT) {
				pctx = ctx;
			}
			continue;
		}

		/* check whether keep-alive timer expired */
        if (ctx->tc_isKeepAliveEnabled &&
            !(ctx->tc_isDeadlineSet && ctx->tc_deadline > 0)) {
            diff = now - ctx->tc_keepAliveStart.tv_sec
                - (ctx->tc_keepAliveStart.tv_usec / 1e6);

            if (diff > ctx->tc_keepAliveTime + ctx->tc_sentProbes * ctx->tc_keepAliveIntvl) {
                if (ctx->tc_sentProbes < ctx->tc_keepAliveProbes) {
                    TRACE("sending a probe packet (retry = %d)\n", ctx->tc_sentProbes);
                    DTPSendProbePacket(ctx);
                    ctx->tc_sentProbes++;
                }
                else {
                    DTPCloseConnection(ctx);
                    continue;
                }
            }
        }

		/* DTP_SO_RCVLOBUF */
		struct in_addr dprox_addr;
		int val = inet_aton(DPROX_IP, &dprox_addr);
		DHK_FLOG(DHK_DEBUG & DRCVLOBUF & 0, DHK_F_BASE"rcvlobuf.txt",
				"%d == %d && %d && %u > 0",
				ctx->tc_state, SOCK_ESTABLISHED,
				val, ctx->tc_rcvLoBufHB);
		if (ctx->tc_state == SOCK_ESTABLISHED
				&& val && ctx->tc_rcvLoBufHB > 0) {
			if (now > prev + 0.1) {
				DHK_TFLOG(DHK_DEBUG & DRCVLOBUF,
						DHK_F_BASE"rcvlobuf.txt",
						"%08X: %s: %d",
						ctx, ctx->tc_limitRcvBuf ? "LIMIT" : "UNLIMIT",
						GetReadBufDataSize(ctx));
				printed = 1;
			}

			switch (ctx->tc_limitRcvBuf) {
				case 0: /* Unlimited */
					/* Use mobile and limit buffering */
					if (ctx->tc_wifiAvailable == 0 ||
							(GetReadBufDataSize(ctx) < ctx->tc_rcvLoBufLB)) {
						if (!dtp_setiface((uint32_t)dprox_addr.s_addr,
									"rmnet0", sizeof("rmnet0"))) {
							ctx->tc_limitRcvBuf = 1;
							DHK_TFLOG(DHK_DEBUG & DRCVLOBUF,
									DHK_F_BASE"rcvlobuf.txt",
									"%08X: set tc_limitRcvBuf as %s",
									ctx, ctx->tc_limitRcvBuf ?
									"LIMIT" : "UNLIMIT");
							DTPReconnect(ctx, 0);
						}
					}
					break;

				case 1: /* Limited */
					/* Use mobile and limit buffering */
					if (ctx->tc_wifiAvailable &&
							GetReadBufDataSize(ctx) > ctx->tc_rcvLoBufHB) {
						if (!dtp_setiface((uint32_t)dprox_addr.s_addr,
								"wlan0", sizeof("wlan0"))) {
							ctx->tc_limitRcvBuf = 0;
							DHK_TFLOG(DHK_DEBUG & DRCVLOBUF,
									DHK_F_BASE"rcvlobuf.txt",
									"%08X: set tc_limitRcvBuf as %s",
									ctx, ctx->tc_limitRcvBuf ?
									"LIMIT" : "UNLIMIT");
							DTPReconnect(ctx, 0);
						}
					}
					break;

				default:
					/* This should never happen */
					break;

			}
		}

		/* persist timer : sends probe packet when win is zero and timer expires*/
		if (ctx->tc_isHerWindowZero) {
			diff = now - ctx->tc_persistTimer.tv_sec
				- (ctx->tc_persistTimer.tv_usec / 1e6);

			if (diff > ctx->tc_persistIntvl) {
				DTPSendProbePacket(ctx);

				/* persist timer : set next probe interval */
				if (gettimeofday(&ctx->tc_persistTimer, NULL)) {
					perror("gettimeofday() failed");
					EXIT(-1, return);
				}

				// exponential backoff
				ctx->tc_persistIntvl = __min(2 * ctx->tc_persistIntvl, DTP_PERSIST_MAX);
			}
		}


		/* check if any packets are in the list */
		if (TAILQ_EMPTY(&ctx->tc_packetQHead))
			continue;

		/* acquire a lock to access the shared variables */
		if (pthread_mutex_lock(&ctx->tc_writeBufLock)) {
			perror("pthread_mutex_lock() failed");
			EXIT(-1, return);
		}

		TAILQ_FOREACH(packet, &ctx->tc_packetQHead, tp_link) {
			/* see if we need to remove a packet from the list */
			if (pdone) {
				TAILQ_REMOVE(&ctx->tc_packetQHead, pdone, tp_link);
				ctx->tc_segSent -= (pdone->tp_len - pdone->tp_headerLen);
				AddToFreePacketList(pdone);
				pdone = NULL;
			}

			/* remove packets if retransmission happened for that context */
			if (packet->tp_ctx->tc_isCongested && !packet->tp_isRetrans) {
				pdone = packet;
				continue;
			}

			diff = now - packet->tp_time.tv_sec - (packet->tp_time.tv_usec / 1e6);
	    
			/* 30 sec timeout for LAST_ACK */
			if ((ctx->tc_state == SOCK_LAST_ACK) && (diff > LAST_TIMEOUT)) {
				/* set the candidate packet to be removed from the list */
				pdone = packet;
				pctx = ctx;
				break;
			}
			/* see if timeout expired */
			else if (diff < ctx->tc_RTO) {
				break;
			}
			else {   
				/* connected: slow start */
				if (ctx->tc_isNetConnected) {
					packet->tp_isRetrans = TRUE;
					ctx->tc_RTO = __min(2 * ctx->tc_RTO, RTO_MAX);
					if (gettimeofday(&packet->tp_time, NULL)) {
						perror("gettimeofday() failed");
						EXIT(-1, );
					}
					ctx->tc_isCongested = TRUE;

					/* remove all packets except this packet */
					ctx->tc_lastByteSent = packet->tp_seqNum + 
						packet->tp_len - packet->tp_headerLen;

					if (ctx->tc_isFINSent && !IsFINPacket(packet->tp_header)) {
						ctx->tc_lastByteWritten -= 1;
						ctx->tc_isFINSent = FALSE;
					}
					TimeoutSlowStart(ctx);
				} 
				else {
					/* disconnected: increment RTO by 1 second (default) */
					ctx->tc_RTO += RETRY_INTVL;
				}
				/* mobile network must be used or mobile network is not connected */
				/* dhkim: add close_wait, fin_wait, last_ack
				 * for to do retransmission */
				if (ctx->tc_state == SOCK_SYN_SENT ||
					ctx->tc_state == SOCK_ESTABLISHED ||
					ctx->tc_state == SOCK_CLOSE_WAIT ||
					ctx->tc_state == SOCK_FIN_WAIT_1 ||
					ctx->tc_state == SOCK_LAST_ACK)
#ifdef IN_MOBILE
			    {
					if (IsAvailConnection(ctx))
						DTPRetransmit(ctx->tc_sock, packet);
				}
#else
				DTPRetransmit(ctx->tc_sock, packet);
#endif
			}
		}
		/* see if we need to remove a packet from the list */
		if (pdone) {
			TAILQ_REMOVE(&ctx->tc_packetQHead, pdone, tp_link);
			ctx->tc_segSent -= (pdone->tp_len - pdone->tp_headerLen);
			AddToFreePacketList(pdone);
			pdone = NULL;
		}
		if (pthread_mutex_unlock(&ctx->tc_writeBufLock)) {
			perror("pthread_mutex_unlock() failed");
			EXIT(-1, );
		}
    }
	if (printed == 1)
		prev = now;
    if (pctx) {
		TAILQ_REMOVE(GetRetransQueue(), pctx, tc_link);
		pctx->tc_state = SOCK_CLOSED;
		DTPCloseContext(pctx);
		pctx = NULL;
    }
    if (pthread_mutex_unlock(GetRetransQueueLock())) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }

}
/*-------------------------------------------------------------------*/
void
DTPCloseConnection(dtp_context *ctx) {

	/* socket's pending error is set to ETIMEDOUT */
	ctx->tc_connTimedOut = TRUE;
					
	// [7] CONN TIMED OUT
	// Keep-alive duration expired -> tear down the connection
	DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ);
	DTPSelectEventSet(ctx->tc_sock, DTP_FD_WRITE);
					
	/* socket connection closed */
	ctx->tc_state = SOCK_CLOSED;
				
	/* let the socket function returns */
	DTPReadBufWakeup(ctx);
	DTPWriteBufWakeup(ctx);
				
	/* remove from retransmission timer queue */
	TAILQ_REMOVE(GetRetransQueue(), ctx, tc_link);
}
/*-------------------------------------------------------------------*/
void
HandleDelayedAckTimeout(void)
{
    dtp_context *pctx;
    
    while ((pctx = TAILQ_FIRST(&g_delayedAckQHead)) != NULL) {
		DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
		DTPSendACKPacket(pctx);
		pctx->tc_numPacketRcvd = 0;
		TAILQ_REMOVE(&g_delayedAckQHead, pctx, tc_delayedAckLink);
    }  
}
/*-------------------------------------------------------------------*/
static void
ProcessAggregatePackets(AdjPktInfo *p) 
{
    int i, len, res;
    u_char * pkt;
    int  hasNewData = FALSE;
    dtp_context *ctx = p->ai_ctx;
    int locked = FALSE;

    /* handle ACK */
    if (p->ai_hasACK && ctx->tc_state != SOCK_TIME_WAIT) 
		HandleACKPacket(ctx, p->ai_maxACK, p->ai_windowSize);

    for (i = 0; i < p->ai_count; i++) {
		pkt = p->ai_pkt[i];
		len = GetPayloadLen(pkt, p->ai_len[i]);
		if (len > 0) {
			if (!locked) {
				/* acquire a lock to access the shared variables */
				if (pthread_mutex_lock(&ctx->tc_readBufLock)) {
					if (ctx->tc_isActive == 0)
						return;
					TRACE("[sock %d] mutex lock failed / isActive = %d\n",
						  ctx->tc_sock, ctx->tc_isActive);
					EXIT(-1, return);
				}
				locked = TRUE;
			}

			if ((res = HandleNormalPacket(ctx, pkt, (uint32_t)len)) > 0) {
				hasNewData = TRUE;
			}
		}
		else if (len == 0 && !IsACKPacket(pkt)) {
			// answer to probe packet (with null data)
			TRACE("sending a ACK for probe packet\n");
			DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
			DTPSendACKPacket(ctx);
		}
    }

    /* start keep-alive timer */
    if (gettimeofday(&ctx->tc_keepAliveStart, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, );
    }
	ctx->tc_sentProbes = 0;

    /* if it's not locked, that implies that these are all ACK packets */
    if (!locked) 
		return;

	/* dhkim: XXX: Is it OK to check only SOCK_CLOSE_WAIT?? */
    if (ctx->tc_state != SOCK_CLOSE_WAIT && 
		ctx->tc_state != SOCK_SYN_RCVD &&
		ctx->tc_state != SOCK_SYN_SENT) {
		if (ctx->tc_sendSYN) {
			ctx->tc_sendSYN = FALSE;
		}
		else {
			DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
			DTPSendACKPacket(ctx);
		}
    }

    //    hasNewData = GetReadBufDataSize(ctx);

    /* unblock the read operation if we wrote some new data here */
	// DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ); // YGMOON
    // [3] new data arrived.
    if (GetReadBufDataSize(ctx) > 0) {
		DHK_FDBG(DHK_DEBUG & DDEADLOCK01, DHK_F_BASE"deadlock01.txt", " ");
		DTPSelectEventSet(ctx->tc_sock, DTP_FD_READ);
		DHK_FDBG(DHK_DEBUG & DDEADLOCK01, DHK_F_BASE"deadlock01.txt", " ");
    }

    if (hasNewData && ctx->tc_isReadBlocked) {
		ctx->tc_isTransportWaiting = TRUE;
		if (pthread_cond_signal(&ctx->tc_readBufCond)) {
			TRACE("pthread_cond_signal failed\n");
			EXIT(-1, );
		}
		if (pthread_cond_wait(&ctx->tc_readBufCond, &ctx->tc_readBufLock)) {
			TRACE_ERR("pthread_cond_wait() failed");
			EXIT(-1, );
		}
		ctx->tc_isTransportWaiting = FALSE;    
    }

    /* release the lock */
    if (pthread_mutex_unlock(&ctx->tc_readBufLock)) {
		TRACE("mutex unlock failed\n");
		EXIT(-1, );
    }
}
/*-------------------------------------------------------------------*/
static void
OnReadEvent(int sock, void *isListen)
{
    u_char buf[MAX_BATCH * DTP_MTU];
    struct sockaddr_in addr[MAX_BATCH];
    socklen_t addrlen = sizeof(addr);
    int len[MAX_BATCH];
    dtp_context *ctx = NULL;
    int i, cnt, off;
    uint32_t old_fid = 0;
    AdjPktInfo api = {0};

    /* receive N packets */
    // FIX: if ACK+FIN are batched, FIN is handled first
    for (cnt = 0, off = 0; cnt < MAX_BATCH; cnt++, off += DTP_MTU) {
		len[cnt] = recvfrom(sock, buf + off, DTP_MTU, 0,
							(struct sockaddr *)&addr[cnt], 
							(socklen_t *)&addrlen);

		if (len[cnt] == -1) {
			if (errno == EAGAIN)
				break;
			TRACE_ERR("recvfrom() failed sock=%d\n", sock);
			return;
		}

		/* for debugging */
#ifdef DEBUGX
		TRACEX("[%d] packet (sock %d) arrived addr=%s port=%d len=%d\n", 
			   cnt, sock, inet_ntoa(addr[cnt].sin_addr), 
			   ntohs(addr[cnt].sin_port), len[cnt]);
		DTPPrintPacket(buf + off, len[cnt]);
#endif
    }
        
    for (i = 0, off = 0; i < cnt; i++, off += DTP_MTU) {
		u_char *pkt = buf + off;
		DHK_MEM(DHK_DEBUG & DPKTHDR, DHK_F_BASE"/packet.txt",
				pkt, sizeof(struct dtp_hdr));

        /* Handle a SYN packet and get the context pointer */
		if (IsSYNPacket(pkt)) {
			/* received SYN+ACK */
			if (isListen == NULL) {
				/* dhkim: report and continue instead of assert */
				ASSERT(IsACKPacket(pkt), );
				ctx = HandleSYNACKPacket(sock, pkt, &addr[i]);
				if (ctx == NULL) {
					continue;
				}
			}
			else {
				ctx = HandleSYNPacket(sock, pkt, &addr[i]); 
				if (ctx == NULL) {
					continue;
				}
			}
	    
			/* remember the previous flowID */
			old_fid = ctx->tc_flowID;
		} 
		else {
			/* Get the context pointer */
			uint32_t fid = ntohl(GetFlowID(pkt));

			if (isListen)  {
				if (ctx == NULL || (old_fid != fid)) 
					ctx = DTPGetContextByFlowID(fid);
				old_fid = fid;
			}  
			else if (ctx == NULL) {
				ctx = DTPGetContextBySocket(sock);
			}

			/* handle a RST packet */
			if (IsRSTPacket(pkt)) {
				/* ignore RST packet with unknown flowID */
				if (ctx != NULL) {
					HandleRSTPacket(ctx, pkt);
				}
				continue;
			} 
	    
			/* send a RST packet if the context doesn't exist */
			if (ctx == NULL || fid != ctx->tc_flowID) {
				TRACE("There is no corresponding context.\n");
				SendRSTPacket(sock, pkt, &addr[i]);
				continue;
			}
		}
	
		/* connection has been expired */
		if (ctx->tc_connTimedOut) {
			continue;
		}

		if (ctx->tc_state == SOCK_CLOSED
				|| ctx->tc_state == SOCK_LISTEN
				|| ctx->tc_state == SOCK_SYN_SENT)
			continue;

		/* challenge-and-response */
		if (IsCHGPacket(pkt)) {
			HandleCHGPacket(ctx, pkt, &addr[i]);
			continue;
		}
		else if (IsRSPPacket(pkt)) {
			HandleRSPPacket(ctx, pkt, &addr[i]);
			continue;
		}
		else if (IsAUTHPacket(pkt)) {
			HandleAUTHPacket(ctx, pkt, &addr[i]);
		}

		/* begin challenge-and-response in case of address change */
		//		if (!(IsSYNPacket(pkt) && isListen == NULL))
		if (!(IsSYNPacket(pkt))) {
			if (ctx->tc_state == SOCK_SYN_RCVD) {
				memcpy(&ctx->tc_peerAddr, &addr[i], sizeof(ctx->tc_peerAddr));
			}
			else if (ctx->tc_state != SOCK_TIME_WAIT) {
				if (memcmp(&ctx->tc_peerAddr, &addr[i], 
						   sizeof(ctx->tc_peerAddr)) != 0) {
					HandleAddressChange(ctx, pkt, &addr[i]);
					continue; 
				}
				else {
					ctx->tc_nonce = 0;
				}
			}
		}

		if (IsFINPacket(pkt)) {
			/* If there exists cumulated ACKs, handle them first. */
			TRACE("Process cumulated packets before HandleFIN. (%d)\n",
				  ctx->tc_sock);
	    
			if (api.ai_ctx)
				ProcessAggregatePackets(&api);
			memset(&api, 0, sizeof(api));
			api.ai_ctx = ctx;
	    
			/* Handling a FIN packet */
			HandleFINPacket(ctx, pkt, len[i]);
		} 
		else {
			if (api.ai_ctx != ctx) {
				if (api.ai_ctx)
					ProcessAggregatePackets(&api);
				memset(&api, 0, sizeof(api));
				api.ai_ctx = ctx;
			}  
			api.ai_pkt[api.ai_count] = pkt;
			api.ai_len[api.ai_count] = len[i];
	    
			/* update the ACK-related field */
			if (IsACKPacket(pkt)) {
				uint32_t ack = ntohl(GetAckNum(pkt));
				api.ai_hasACK = TRUE;
				if (api.ai_maxACK <= ack) {
					api.ai_maxACK = ack;
					api.ai_windowSize = ntohs(GetRcvdWindow(pkt));
				}
			}
			api.ai_count++;
		}
    }

    if (api.ai_ctx)
		ProcessAggregatePackets(&api);

}
/*-------------------------------------------------------------------*/
static void
DTPReadEvent(int sock, short event, void *isListen)
{
    /* packet receive */
    if (event & EV_READ)
		OnReadEvent(sock, isListen);
}
/*-------------------------------------------------------------------*/
static void
DTPTimeoutEvent(int sock, short event, void *arg)
{
    /* timer timeout */
    if (event & EV_TIMEOUT) {
		/* timeout for retransmission */
		HandleTimeout();
	}
}
/*-------------------------------------------------------------------*/
static void 
DTPMainThreadEvent(int sock, short events, void *arg) 
{
    dtp_event ev;
    int res;

    if ((res = read(sock, &ev, sizeof(ev))) != sizeof(ev)) {
		TRACE("read() ret=%d failed\n", res);
		EXIT(-1, return);
    }

    switch (ev.te_command) {
    case DTP_ADD_READ_EVENT: 
    case DTP_ADD_LISTEN_EVENT:
		{
			struct event *read_event;
			int isListen = (ev.te_command == DTP_ADD_LISTEN_EVENT);
			dtp_context *ctx;
    	    int res;

			read_event = event_new(arg, ev.te_fd, EV_READ|EV_PERSIST,
								   DTPReadEvent, isListen ? (void *)0x8 : NULL);
			if (!read_event) {
				TRACE("event_new failed\n");
				EXIT(-1, return);
			}

			ctx = DTPGetContextBySocket(ev.te_fd);
			ASSERT(ctx, return);
			ctx->tc_readEvent = read_event;

			res = event_add(read_event, NULL);

			ASSERT(res == 0, );
		}
		break;

    case DTP_REMOVE_EVENT: 
		ASSERT(false, );
		break;

	default:
		break;
    }
}

/*-------------------------------------------------------------------*/
struct event_base *base;
struct event_base *GetEventBase()
{
    return base;
}


void* 
DTPLibThreadMain(void *arg) 
{
    struct event *read_event, *timeout_event;
    int fd = *(int *)arg;
    int res;
    struct timeval tv;
	
	LOGD("DTPLibThreadMain");

	/* ignore SIGPIPE to handle locally */
	signal(SIGPIPE, SIG_IGN);

    /* initialize timer lock */
	RetransQueueLockInit();

	FreeBufferLockInit();

    /* idle packet initialization */
    DTPInitializeIdlePacket();

    /* initialize the flow ID hash table */
    InitializeFlowIDMap();
	    
    /* generate base event */
    base = event_base_new();
    ASSERT(base, return NULL);

    /* pass the inner_sock to dtp_select */
    DTPSetInnerSock(fd);
 
    /* generate events */
    read_event = event_new(base, fd, EV_READ|EV_PERSIST, 
						   DTPMainThreadEvent, (void *)base);
    ASSERT(read_event, return NULL);
    event_add(read_event, NULL);

    /* timeout timer (0.1 of RTO_MIN) */
    tv.tv_sec = 0;
    tv.tv_usec = (0.1 * RTO_MIN) * 1000 * 1000;

    timeout_event = event_new(base, -1, EV_PERSIST, DTPTimeoutEvent, NULL);
    if (!timeout_event) {
		TRACE("event_new failed\n");
		EXIT(-1, return NULL);
    }
    res = event_add(timeout_event, &tv);
    if (res) {
		TRACE("event_add failed\n");
		EXIT(-1, return NULL);
    }

	
#ifdef IN_MOBILE
	DTPMobileEventInit(base);
	/* Use WIFI at start */
//	dtp_setiface(DPROX_IP_HEX, "wlan0", 5);
#endif

    event_base_dispatch(base);

    /* can't reach here */
    return(NULL);
}
/*-------------------------------------------------------------------*/

void DTPPrintCtx(FILE *fp, dtp_context* ctx)
{
	if (ctx == NULL)
		return;

	DHK_PRINT(ctx->tc_state, "%d");
	DHK_PRINT(ctx->tc_fstatus, "%d");
	DHK_PRINT(ctx->tc_sock, "%d");
	DHK_PRINT(ctx->tc_isock, "%d");

	DHK_PRINT(ctx->tc_readEvent, "%p");

	DHK_PRINT(ctx->tc_waitTime.tv_sec, "%lu");
	DHK_PRINT(ctx->tc_waitTime.tv_usec, "%lu");

	DHK_PRINT(ctx->tc_writeBuf, "%p");
	DHK_PRINT(ctx->tc_writeBufLen, "%d");
	DHK_PRINT(ctx->tc_lastByteAcked, "%08X");
	DHK_PRINT(ctx->tc_lastByteWritten, "%08X");
	DHK_PRINT(ctx->tc_lastByteSent, "%08X");
	
	DHK_PRINT(ctx->tc_readBuf, "%p");
	DHK_PRINT(ctx->tc_readBufHead, "%p");
	DHK_PRINT(ctx->tc_readBufLen, "%d");
	DHK_PRINT(ctx->tc_lastByteRead, "%08X");
	DHK_PRINT(ctx->tc_nextByteExpected, "%08X");
	DHK_PRINT(ctx->tc_lastByteRcvd, "%08X");

	DHK_PRINT(ctx->tc_flowID, "%08X");
	DHK_PRINT(ctx->tc_seqNum, "%08X");

	DHK_PRINT(ctx->tc_numPacketRcvd, "%d");
	DHK_PRINT(ctx->tc_rcvWindow, "%08X");

    DHK_PRINT(ctx->tc_isBound, "%d");
    DHK_PRINT(ctx->tc_isReadBlocked, "%d");
    DHK_PRINT(ctx->tc_isWriteBlocked, "%d");
    DHK_PRINT(ctx->tc_isQueueBlocked, "%d");
    DHK_PRINT(ctx->tc_isActive, "%d");
    DHK_PRINT(ctx->tc_closeCalled, "%d");
    DHK_PRINT(ctx->tc_isFirstPacketSent, "%d");
    DHK_PRINT(ctx->tc_isFirstDataSent, "%d");
    DHK_PRINT(ctx->tc_isSYNSent, "%d");
    DHK_PRINT(ctx->tc_isFINSent, "%d");
    DHK_PRINT(ctx->tc_isFINRcvd, "%d");          /* set to true if received FIN after the whole data */
    DHK_PRINT(ctx->tc_sendSYN, "%d");
    DHK_PRINT(ctx->tc_recvAnyData, "%d");
    DHK_PRINT(ctx->tc_waitACK, "%d");            /* wait for ACK */
    DHK_PRINT(ctx->tc_sendACK, "%d");            /* send ACK */
    DHK_PRINT(ctx->tc_isCongested, "%d");        /* retransmission due to congestion */
    DHK_PRINT(ctx->tc_useWindowScale, "%d");     /* use window scale option or not */
    DHK_PRINT(ctx->tc_usePAWS, "%d");            /* protect against wrap-around sequence */
    DHK_PRINT(ctx->tc_useLinger, "%d");          /* linger option */
    DHK_PRINT(ctx->tc_isNetConnected, "%d");     /* check network status */
    DHK_PRINT(ctx->tc_isMyWindowZero, "%d");     /* is my window size 0? */
    DHK_PRINT(ctx->tc_isHerWindowZero, "%d");    /* is her window size 0? */
    DHK_PRINT(ctx->tc_isTransportWaiting, "%d"); /* is transport thread waiting for read? */
    DHK_PRINT(ctx->tc_onFlowIDMap, "%d");        /* whether it's on the Flow ID Map or not */
	DHK_PRINT(ctx->tc_isKeepAliveEnabled, "%d"); /* keep-alive time is enabled */
    DHK_PRINT(ctx->tc_connTimedOut, "%d");
    DHK_PRINT(ctx->tc_connReset, "%d");
    DHK_PRINT(ctx->tc_beingMonitored, "%d");     /* check whether the sock is added to libthread */
    DHK_PRINT(ctx->tc_isACKInserted, "%d");      /* is ACK inserted in the packet queue? */
    DHK_PRINT(ctx->tc_isAUTHSent, "%d");         /* is AUTH sent? */
    DHK_PRINT(ctx->tc_isDeadlineSet, "%d");      /* is deadline set? */
    DHK_PRINT(ctx->tc_isDeadlineChanged, "%d");  /* is deadline changed? */
    DHK_PRINT(ctx->tc_isWifiOnly, "%d");         /* use mobile network */
	DHK_PRINT(ctx->tc_isClientMobile, "%d");     /* is client in mobile network? */
	DHK_PRINT(ctx->tc_scheduleSend, "%d");       /* did scheduler allow send? */
	DHK_PRINT(ctx->tc_schedSendFail, "%d");      /* failed to send message to scheduler */
    DHK_PRINT(ctx->tc_isMobileBWMeasuring, "%d");
	DHK_PRINT("-----", "%s");
}
