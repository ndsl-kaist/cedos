/*
  dtp.c : basic functions for DTP protocol
*/
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdbool.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include "dtp_transport.h"
#include "dtp_socket.h"
#include "debug.h"
#include "dtp.h"
#include "dhkim_debug.h"

/*--------------------------------------------------------------------*/
void
err_exit(char* message)
{
    fprintf(stderr,"%s", message);
    EXIT(1, return);
}
/*-------------------------------------------------------------------*/
void
DTPGenerateHostID(u_char* hostID)
{
#ifdef ANDROID
    /* Host ID = SHA-1(IMEI) */
    // retrieve IMEI from the phone
	char* IMEI = GetIMEI();
	int IMEI_len = GetIMEILength();

    SHA_CTX c;

    if (SHA1_Init(&c) == 0) {
		TRACE("SHA1_Init failed\n");
		EXIT(-1, return);
    }
    if (SHA1_Update(&c, IMEI, IMEI_len) == 0) {
		TRACE("SHA1_Update failed\n");
		LOGE("SHA1_Update failed\n");
		EXIT(-1, return);
    }
    if (SHA1_Final(hostID, &c) == 0) {
		TRACE("SHA1_Final failed\n");
		EXIT(-1, return);
    }
#else 
    /* Host ID = SHA-1(MAC address of the first interface) */
    unsigned char MAC[6];
    int MAC_len = 6;
    SHA_CTX c;
    struct ifreq ifr;
    struct ifconf ifc;
    char buf[1024];

    /* retrieve MAC address of first interface */
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) {
		TRACE("socket() error\n");
		EXIT(-1, return);
    };

    ifc.ifc_len = sizeof(buf);
    ifc.ifc_buf = buf;
    if (ioctl(sock, SIOCGIFCONF, &ifc) == -1) { 
		TRACE("ioctl() error\n");
    }
    
    struct ifreq* it = ifc.ifc_req;
    struct ifreq* end = it + (ifc.ifc_len / sizeof(struct ifreq));

    for (; it != end; ++it) {
        strcpy(ifr.ifr_name, it->ifr_name);
        if (ioctl(sock, SIOCGIFFLAGS, &ifr) == 0) {
			/* don't count loopback */
            if (!(ifr.ifr_flags & IFF_LOOPBACK)) { 
                if (ioctl(sock, SIOCGIFHWADDR, &ifr) == 0) {
					memcpy(MAC, ifr.ifr_hwaddr.sa_data, 6);
                    break;
                }
            }
        }
    }

    close(sock);

    /* calculate SHA-1 hash */
    if (SHA1_Init(&c) == 0) {
		TRACE("SHA1_Init failed\n");
		EXIT(-1, return);
    }
    if (SHA1_Update(&c, MAC, MAC_len) == 0) {
		TRACE("SHA1_Update failed\n");
		EXIT(-1, return);
    }
    if (SHA1_Final(hostID, &c) == 0) {
		TRACE("SHA1_Final failed\n");
		EXIT(-1, return);
    }
#endif
}
/*-------------------------------------------------------------------*/
uint32_t
DTPGenerateFlowID(const u_char* hostID)
{
    /* FlowID = SHA1(HostID | CurrentTime) */
    struct timeval curtime;
    unsigned char digest[SHA1_DIGEST_LENGTH] = {'\0'};
    uint32_t flowid;
    SHA_CTX c;
    pid_t pid = getpid();

    gettimeofday(&curtime, NULL);

    if (SHA1_Init(&c) == 0) {
		TRACE("SHA1_Init failed\n");
		EXIT(-1, return 0xFFFFFFFF);
    }
    if (SHA1_Update(&c, &pid, sizeof(pid_t)) == 0) {
		TRACE("SHA1_Update failed\n");
		EXIT(-1, return 0xFFFFFFFF);
    }
    if (SHA1_Update(&c, hostID, SHA1_DIGEST_LENGTH) == 0) {
		TRACE("SHA1_Update failed\n");
		EXIT(-1, return 0xFFFFFFFF);
    }
    if (SHA1_Update(&c, &(curtime.tv_sec), sizeof(long)) == 0) {
		TRACE("SHA1_Update failed\n");
		EXIT(-1, return 0xFFFFFFFF);
    }
    if (SHA1_Update(&c, &(curtime.tv_usec), sizeof(long)) == 0) {
		TRACE("SHA1_Update failed\n");
		EXIT(-1, return 0xFFFFFFFF);
    }
    if (SHA1_Final(digest, &c) == 0) {
		TRACE("SHA1_Final failed\n");
		EXIT(-1, return 0xFFFFFFFF);
    }

    memcpy(&flowid, digest + SHA1_DIGEST_LENGTH - 4, 4);

    return flowid;
}
/*--------------------------------------------------------------------*/
int
DTPGenerateHeader(u_char *p, uint32_t seq_num, uint32_t ack_seq_num, 
				  uint32_t flowid, int flag, uint32_t win_size)
{
    struct dtp_hdr *hdr;
    int hdr_size;

    ASSERT(p, return -1);
    hdr = (struct dtp_hdr *)p;
    hdr_size = sizeof(struct dtp_hdr);

    /* flow ID, SEQ, ACK */
    hdr->fid = htonl(flowid);
    hdr->seq = htonl(seq_num);
    hdr->ack_seq = htonl(ack_seq_num);
	
    /* flag */
    hdr->chg = (flag & DTP_FLAG_CHG)? 1 : 0;
    hdr->rsp = (flag & DTP_FLAG_RSP)? 1 : 0;
    hdr->auth = (flag & DTP_FLAG_AUTH)? 1 : 0;
    //hdr->urg = (flag & DTP_FLAG_URG)? 1 : 0;
    hdr->ack = (flag & DTP_FLAG_ACK)? 1 : 0;
    //hdr->psh = (flag & DTP_FLAG_PSH)? 1 : 0;
    hdr->rst = (flag & DTP_FLAG_RST)? 1 : 0;
    hdr->syn = (flag & DTP_FLAG_SYN)? 1 : 0;
    hdr->fin = (flag & DTP_FLAG_FIN)? 1 : 0;
#ifdef IN_MOBILE
	hdr->mobile = (flag & DTP_FLAG_MOBILE)? 1 : 0;
#endif

	DHK_FDBG(DHK_DEBUG & DRCVLOBUF & 0, DHK_F_BASE"window.txt",
			"win_size = %u", win_size);
    hdr->window_size = htons(win_size > 0x7FFF ? 0x7FFF : win_size);
    hdr->doff = ((hdr_size + 3) >> 2);

    return hdr_size;
}

/*--------------------------------------------------------------------*/
int
DTPAddOptionToHeader(u_char *p, int offset, uint8_t opcode, void* op_ptr)
{
    struct dtp_hdr *hdr;
    int hdr_size;
    uint8_t opsize;

    hdr = (struct dtp_hdr *)p;

    switch (opcode) {

    case DTPOPT_MSS:
		opsize = DTPOLEN_MSS;
		break;

    case DTPOPT_WIN_SCALE:
		opsize = DTPOLEN_WIN_SCALE;
		break;

    case DTPOPT_SACK_PERM:
		opsize = DTPOLEN_SACK_PERM;
		break;

    case DTPOPT_SACK:
		// FIX : add SACK option with variable length.
		ASSERT(0, break);
		break;

    case DTPOPT_HOST_ID:
		opsize = DTPOLEN_HOST_ID;
		break;

    case DTPOPT_PORT:
		opsize = DTPOLEN_PORT;
		break;

    case DTPOPT_DEADLINE:
		opsize = DTPOLEN_DEADLINE;
		break;

    default:
		ASSERT(0, break);
		break;
    }

    p[offset]  = (char)opcode;
    p[offset + 1] = (char)opsize;

    if (opsize > 2) {
		ASSERT(op_ptr, return -1);
		memcpy(&p[offset + 2], op_ptr, opsize - 2);
    }
    
    hdr_size = offset + opsize;    
    memset(&p[hdr_size], 0, 4 - (hdr_size & 0x03));
    hdr->doff = ((hdr_size + 3) >> 2);

    return hdr_size;
}
/*-------------------------------------------------------------------*/
static inline uint32_t
GetFlowID(const u_char *p)
{
    /* we assume hdr is in network byte order */
    return (((struct dtp_hdr *)p)->fid);
}

/*--------------------------------------------------------------------*/
int
DTPGetOption(u_char* buf, u_char *host_id, uint32_t *deadline)
{
    u_char* ptr;
    int len;
    uint8_t opcode, opsize;

    ptr = buf + (sizeof(struct dtp_hdr));
    len = (((struct dtp_hdr *)buf)->doff << 2) - (sizeof(struct dtp_hdr));
	
    while (len > 0) {
		opcode = (uint8_t)*ptr++;
		
		if (opcode == DTPOPT_EOL)
			break;
		
		if (opcode == DTPOPT_NOP) {
			len--;
			continue;
		}
		
		opsize = (uint8_t)*ptr++;
		
		switch(opcode) {
		case DTPOPT_HOST_ID:
			if (opsize == DTPOLEN_HOST_ID) {
				if (host_id != NULL)
					memcpy(host_id, ptr, SHA1_DIGEST_LENGTH);
			}
			break;
		case DTPOPT_DEADLINE:
			if (opsize == DTPOLEN_DEADLINE) {
				memcpy(deadline, ptr, sizeof(uint32_t));
			}
			break;
		default:
			break;
		}
		
		ptr += (opsize - 2);
		len -= opsize;
    }
	if (host_id == NULL) {
		TRACE("received no host ID\n");
		return FALSE;
    }
	
    return TRUE;
}


/*--------------------------------------------------------------------*/
void
DTPParseOption(u_char* buf, const void* this_ctx, int isSYNPacket)
{
    u_char* ptr;
    int opt_len;
    uint8_t opcode, opsize;
    dtp_context* ctx;
    uint8_t advertisedWindowScale;	
	u_short port;

    ctx = (dtp_context*) this_ctx;

    ptr = buf + (sizeof(struct dtp_hdr));
    opt_len = ((((struct dtp_hdr *)buf)->doff) << 2) - (sizeof(struct dtp_hdr));

    while (opt_len > 0) {
		opcode = (uint8_t)(*(ptr++));

		if (opcode == DTPOPT_EOL)
			break;
		if (opcode == DTPOPT_NOP) {
			opt_len--;
			continue;
		}

		opsize = (uint8_t)(*(ptr++));

		switch(opcode) {
			/* FIX: consider isSYNPacket when parsing options */
		case DTPOPT_WIN_SCALE:
			if (opsize == DTPOLEN_WIN_SCALE) {
				advertisedWindowScale = (uint8_t)*ptr;
				if (advertisedWindowScale <= DTP_MAX_WINSCALE_SHIFT) {
					ctx->tc_sendWindowScale = advertisedWindowScale;
					TRACE("advertised win scale : %d\n", ctx->tc_sendWindowScale);
				}
			}
			break;
		case DTPOPT_PORT:
			if (opsize == DTPOLEN_PORT) {
				memcpy(&port, ptr, 2);
				ctx->tc_peerAddr.sin_port = htons(port);			
			}
		default:
			break;
		}
	
		ptr += (opsize - 2);
		opt_len -= opsize;
    }
}


/* free buffer queue */
#define NUM_BATCH       1024
#define BATCH_SIZE_BITS 12
#define BATCH_SIZE      (1 << BATCH_SIZE_BITS)
static TAILQ_HEAD(, buffer) g_freeBufferList = 
	TAILQ_HEAD_INITIALIZER(g_freeBufferList);
static int g_freeBufferCount = 0;
static pthread_mutex_t g_freeBufferLock;
/*-------------------------------------------------------------------*/
void
FreeBufferLockInit() {
    if (pthread_mutex_init(&g_freeBufferLock, NULL)) {
		TRACE_ERR("pthread_mutex_init() failed\n");
		EXIT(-1, return);
    }
}
/*-------------------------------------------------------------------*/
void
AddToFreeBufferList(buffer *b)
{
    if (pthread_mutex_lock(&g_freeBufferLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
	b->seq = 0;
	b->len = 0;
	b->next_buf = NULL;

    TAILQ_INSERT_HEAD(&g_freeBufferList, b, link);
    g_freeBufferCount++;
    if (pthread_mutex_unlock(&g_freeBufferLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }
}
/*-------------------------------------------------------------------*/
static void
AllocateMoreBuffer(void)
{
    buffer *b;
    int i;
    
    for (i = 0; i < BATCH_SIZE; i++) {
		if ((b = calloc(1, sizeof(struct buffer))) == NULL) {
			fprintf(stderr, "calloc() error\n");
			EXIT(-1, return);
		}
		TAILQ_INSERT_HEAD(&g_freeBufferList, b, link);
		g_freeBufferCount++;
		//		AddToFreeBufferList(b);
    }
}
/*-------------------------------------------------------------------*/
static buffer *
RemoveFromFreeBufferList(void)
{
    buffer *b;

    if (pthread_mutex_lock(&g_freeBufferLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return NULL);
    }

    b = TAILQ_FIRST(&g_freeBufferList);
    if (b == NULL) {
		AllocateMoreBuffer();
		b = TAILQ_FIRST(&g_freeBufferList);
    }

    TAILQ_REMOVE(&g_freeBufferList, b, link);
    g_freeBufferCount--;

    if (pthread_mutex_unlock(&g_freeBufferLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }

    return(b);
}
/*-------------------------------------------------------------------*/
buffer *
GetBuffer(void) 
{
    return RemoveFromFreeBufferList();
}

