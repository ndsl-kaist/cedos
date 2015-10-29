#ifndef _DTP_TRANSPORT_H_
#define _DTP_TRANSPORT_H_

#include "queue.h"
#include "dtp.h"
#include "crypt.h"

#include <pthread.h>
#include <fcntl.h>

#define FREE_MEM(x) if (x) {free(x); (x) = NULL;}

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))

#define TRUE 1
#define FALSE 0

/* DTP predefined parameters */
#define DH_OPTION_LEN          8
#define SHA1_DIGEST_LENGTH     20
#define RTO                    1.5
#define RTO_MIN                1
#define RTO_MAX                64.0
#define RTO_DISCONNECTED       30.0

#define IFACE_EVAL_TIME        1 /* evaluation time of an interface (RTT) */

#define DTP_KEEPALIVE_TIME     7200
#define DTP_KEEPALIVE_PROBES   9
#define DTP_KEEPALIVE_INTVL    75

#define DTP_PERSIST_MIN        5
#define DTP_PERSIST_MAX        60

#define DTP_MTU                1472      /* FIX: need to readjust the size */
#define DTP_MAX_WINSCALE_SHIFT 14        /* max window scale shift amount is 14 */

#define LAST_TIMEOUT           30    /* 30 sec timeout for last ACK & time wait */

#define INIT_MOBILE_SPEED      0

/* tf socket state */
enum { 
    SOCK_CLOSED, 
    SOCK_LISTEN,
    SOCK_SYN_RCVD, 
    SOCK_SYN_SENT, 
    SOCK_ESTABLISHED,
    SOCK_FIN_WAIT_1, 
    SOCK_FIN_WAIT_2, 
    SOCK_TIME_WAIT,
    SOCK_CLOSE_WAIT, 
    SOCK_LAST_ACK, 
};

/* dtp_event commands */
#define DTP_ADD_READ_EVENT      0
#define DTP_REMOVE_EVENT        1
#define DTP_ADD_LISTEN_EVENT    2
#define DTP_ADD_SCHEDULER_EVENT 3
#define DTP_FD_READ             0
#define DTP_FD_WRITE            1

typedef struct dtp_event
{
    int te_fd;
    int te_command;
} dtp_event;


/*  dtp_context
 *  Includes most of this is sock/network layer working state.
 *  There exists one instance of this structure per DTP socket.
 */
typedef struct dtp_context
{
    /* state for the DTP connection */
    int tc_state;

    /* file status flag for DTP socket */
    int tc_fstatus;

    /* DTP socket number for this context (same as UDP socket) */
    int tc_sock;

    /* the inner socket */
    int tc_isock;

    /* event */
    struct event *tc_readEvent;

    /* key */
    unsigned char tc_key[RSA_LEN];
    int tc_keyLen;

    /* TIME_WAIT */
    struct timeval tc_waitTime;

    /* address of the peer */
    struct sockaddr_in tc_peerAddr;
    int tc_nonce;

    /* write socket buffer */
    u_char *tc_writeBuf;
    int tc_writeBufLen;       /* allocated size */
    uint32_t tc_lastByteAcked;
    uint32_t tc_lastByteWritten;
    uint32_t tc_lastByteSent; /* last byte that is sent */
    
    /* read socket buffer */
    u_char *tc_readBuf;
	struct buffer *tc_readBufHead; 
    int tc_readBufLen;       /* allocated size */
    uint32_t tc_lastByteRead;
    uint32_t tc_nextByteExpected;
    uint32_t tc_lastByteRcvd;

	uint32_t tc_rcvLoBufHB;
	uint32_t tc_rcvLoBufLB;

    /* timeout timer */
    double tc_estRTT;
    double tc_devRTT;
    double tc_RTO;

    /* keep-alive variables */
    int tc_keepAliveTime;   // required idle time to send probes
    int tc_keepAliveIntvl;  // interval time between probes
    int tc_keepAliveProbes; // number of probes to be sent
    struct timeval tc_keepAliveStart;  // keep-alive timer
    int tc_sentProbes;      // number of probes that were sent	

    /* locks and cond variables */
    pthread_mutex_t tc_readBufLock;
    pthread_cond_t tc_readBufCond;
    pthread_mutex_t tc_writeBufLock;
    pthread_cond_t tc_writeBufCond;
    pthread_cond_t tc_closeCond;
    
    /* connection queue */
    int tc_backlog;    /* max # of pending connections */
    int tc_qidx;       /* current idx */
    int *tc_listenQ;   /* connection array */
    
    // bit flags
    uint64_t tc_isBound:1;
    uint64_t tc_isReadBlocked:1;
    uint64_t tc_isWriteBlocked:1;
    uint64_t tc_isQueueBlocked:1;
    uint64_t tc_isActive:1;
    uint64_t tc_closeCalled:1;
    uint64_t tc_isFirstPacketSent:1;
    uint64_t tc_isFirstDataSent:1;
    uint64_t tc_isSYNSent:1;
    uint64_t tc_isFINSent:1;
    uint64_t tc_isFINRcvd:1;          /* set to true if received FIN after the whole data */
    uint64_t tc_sendSYN:1;
    uint64_t tc_recvAnyData:1;
    uint64_t tc_waitACK:1;            /* wait for ACK */
    uint64_t tc_sendACK:1;            /* send ACK */
    uint64_t tc_isCongested:1;        /* retransmission due to congestion */
    uint64_t tc_useWindowScale:1;     /* use window scale option or not */
    uint64_t tc_usePAWS:1;            /* protect against wrap-around sequence */
    uint64_t tc_useLinger:1;          /* linger option */
    uint64_t tc_isNetConnected:1;     /* check network status */
    uint64_t tc_isMyWindowZero:1;     /* is my window size 0? */
    uint64_t tc_isHerWindowZero:1;    /* is her window size 0? */
    uint64_t tc_isTransportWaiting:1; /* is transport thread waiting for read? */
    uint64_t tc_onFlowIDMap:1;        /* whether it's on the Flow ID Map or not */
	uint64_t tc_isKeepAliveEnabled:1; /* keep-alive time is enabled */
    uint64_t tc_connTimedOut:1;
    uint64_t tc_connReset:1;
    uint64_t tc_beingMonitored:1;     /* check whether the sock is added to libthread */
    uint64_t tc_isACKInserted:1;      /* is ACK inserted in the packet queue? */
    uint64_t tc_isAUTHSent:1;         /* is AUTH sent? */
    uint64_t tc_isDeadlineSet:1;      /* is deadline set? */
    uint64_t tc_isDeadlineChanged:1;  /* is deadline changed? (needs to be advertised to other end) */
    uint64_t tc_useMobile:1;          /* use mobile network */
    uint64_t tc_isWifiOnly:1;         /* use mobile network */
	uint64_t tc_isClientMobile:1;     /* is client in mobile network? */
	uint64_t tc_scheduleSend:1;       /* did scheduler allow send? */
	uint64_t tc_schedSendFail:1;      /* failed to send message to scheduler */
    uint64_t tc_isMobileBWMeasuring:1;
	uint64_t tc_limitRcvBuf:1;
	uint64_t tc_mobileAvailable:1;
	uint64_t tc_wifiAvailable:1;
	uint64_t tc_doingCHGRSP:1;

    /* DTP semantics */
    uint32_t  tc_flowID;
    u_char    tc_hostID[SHA1_DIGEST_LENGTH];
    uint32_t  tc_seqNum;
    uint32_t  tc_ackNum;

    /* delayed ACK */
    int tc_numPacketRcvd; /* number of packets received */

    /* duplicate ACK */
    int tc_numACKRcvd;

    /* flow control */
    uint32_t tc_rcvWindow;

    /* congestion control */
    int tc_cwnd;
    int tc_ssthresh;
    int tc_segSent;

    /* window scale option */
    uint8_t tc_recvWindowScale;  /* shift amount for recv window scale */
    uint8_t tc_sendWindowScale;  /* shift amount for send window scale */

	/* persist timer (recovery from window zero) */
    struct timeval tc_persistTimer;
	int tc_persistIntvl;                // interval time between persist probes

    /* packet queue */
    TAILQ_HEAD(/*useless*/, dtp_pkt) tc_packetQHead;

    /* deadline related */
    uint32_t tc_blockSize;
    uint32_t tc_blockRemain;
    int tc_deadline;
    struct timeval tc_deadlineTime;

	double tc_mobileSpeed;	/* mobile timer for measuring BW */
	int tc_mobileCounter;
	uint32_t tc_lastBlockRemain;

	/* scheduler related */
#ifdef HAVE_SCHEDULER
	int tc_schedStatus;           /* is flow in progress or end? */
	int tc_schedLink;             /* up/downlink info for scheduler */
	//	struct timeval tc_schedTime;
	//	int tc_schedSock;
	//    struct event *tc_schedEvent;
#endif

    uint32_t tc_blockRemainLast;
    double tc_networkTimeLast;
    int tc_schedAllowTime;
	//    int tc_networkTypeLast;

	/* mobile/wifi connection time for logging */
	struct timeval tc_startTime;
	pthread_mutex_t tc_connTimeLock;
	long long int tc_mobileTime;
	long long int tc_wifiTime;

	/* up/down byte */
	uint32_t tc_upByte;     /* upload from device */
	uint32_t tc_downByte;   /* download to device */
	pthread_mutex_t tc_upByteLock;
	pthread_mutex_t tc_downByteLock;

    TAILQ_ENTRY(dtp_context) tc_link;
    TAILQ_ENTRY(dtp_context) tc_delayedAckLink;
    TAILQ_ENTRY(dtp_context) tc_flowIDMapLink;
} dtp_context;

/* packet queue */
typedef struct dtp_pkt
{
    dtp_context *tp_ctx; /* pointer to context for RTO */
    uint32_t tp_seqNum;
    int tp_len;
    u_char tp_header[2048]; 
    int tp_headerLen;
    struct timeval tp_time;

    // bit flags
    uint8_t tp_isRetrans:1;

    TAILQ_ENTRY(dtp_pkt) tp_link;
    TAILQ_ENTRY(dtp_pkt) tp_freeLink;
} dtp_pkt;

/*
 *  DTPCreateContext() - Create a new dtp_context.
 *                       Create a new UDP socket and link to context.
 *                       Initialize context related variables.
 *                       Initialize pthread related variables.
 *
 *  fd: socket context id. - if 0, create new physical socket.
 */
extern dtp_context* DTPCreateContext();

/*
 *  DTPGetContext() - Return the address of corresponding context.
 *
 *  sock: socket context id.
 */
extern dtp_context* DTPGetContext(int sock);

/*
 *  DTPLibThreadMain() - Main library thread made from socket().
 *
 *  arg: socket to listen on.
 */
extern void* DTPLibThreadMain(void *arg);

/*
 *  DTPSendEventToLibThread() - Send event to the main library thread.
 *
 *  sock: socket to the library thread.
 *  ev: event.
 */
extern void DTPSendEventToLibThread(int sock, dtp_event *ev);

/*
 *  DTPWriteAvail() - Send DTP packets to the other host from the 
 *  receive buffer.
 *
 *  sock: UDP socket.
 *  ctx: context.
 */
extern void DTPWriteAvail(int sock, dtp_context *ctx);

/*
 *  DTPSendFINPacket() - Send FIN packet to close connection
 *
 *  ctx: context.
 */
extern void DTPSendFINPacket(dtp_context *ctx);

/*
 *  DTPSendACKPacket() - Send ACK packet to sender
 *
 *  ctx: context.
 */
extern int DTPSendACKPacket(dtp_context *ctx);

/*
 *  DTPSendProbePacket() - Send probe packet to sender
 *
 *  ctx: context.
 */
extern void DTPSendProbePacket(dtp_context *ctx);

/*
 *  DTPCloseContext() - Close all of the context's settings.
 *
 *  ctx: context.
 */
extern void DTPCloseContext(dtp_context *ctx);

/*
 *  DTPCloseConnection() - Close the connection
 *
 *  ctx: context.
 */
extern void DTPCloseConnection(dtp_context *ctx);

/*
 *  DTPIsValidSockfd() - Check if it is valid(active) socket descriptor
 *
 */
extern bool DTPIsValidSockfd(int sock);

extern RSA * DTPInitializeRSAKey();

extern void HandleDisconnect(dtp_context *ctx);

extern pthread_mutex_t* GetTimerLock();

extern struct tailQ* GetRetTimerQ();

extern struct event_base *GetEventBase();

extern void DTPPrintCtx(FILE *fp, dtp_context* ctx);

int dtp_setiface(uint32_t dest, char *buf, size_t len);

/* inline functions */
/* write buffer helper functions */
static inline u_char* GetWriteBufLastByteSentPtr(dtp_context* ctx)
{
    return ctx->tc_writeBuf + 
	(ctx->tc_lastByteSent - ctx->tc_lastByteAcked);
}
static inline u_char* GetWriteBufPacketSentPtr(dtp_pkt* packet)
{
    /* ensure that return value points valid position on tc_writeBuf */
	//    assert(packet->tp_ctx->tc_lastByteAcked <= packet->tp_seqNum);

    return packet->tp_ctx->tc_writeBuf + 
	(packet->tp_seqNum - packet->tp_ctx->tc_lastByteAcked);
}
static inline int GetWriteBufOff(dtp_context* ctx)
{
	return (ctx->tc_lastByteWritten - ctx->tc_lastByteAcked);
}
static inline int GetAvailWriteBufSize(dtp_context *ctx)
{
	return (ctx->tc_writeBufLen - GetWriteBufOff(ctx));
}
/* read buffer header functions */
static inline int GetReadBufDataSize(dtp_context *ctx)
{
	return (ctx->tc_nextByteExpected - ctx->tc_lastByteRead);
}
static inline uint32_t GetAvailReadBufSize(dtp_context *ctx) 
{
	/* return available buffer size */
	return (ctx->tc_readBufLen - GetReadBufDataSize(ctx));
}
/* returns acknowledge number for the context */
static inline uint32_t GetAckNumber(dtp_context *ctx)
{
    return (ctx->tc_nextByteExpected + (ctx->tc_isFINRcvd ? 1 : 0));
}
/* window scale */
static inline void UseWindowScale(dtp_context *ctx, uint8_t recvWindowScale) 
{
    ctx->tc_useWindowScale = TRUE;
    ctx->tc_recvWindowScale = recvWindowScale;

    if (ctx->tc_recvWindowScale > DTP_MAX_WINSCALE_SHIFT)
	ctx->tc_recvWindowScale = DTP_MAX_WINSCALE_SHIFT;
}

#endif
