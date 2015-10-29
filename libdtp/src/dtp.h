#ifndef __DTP_H__
#define __DTP_H__

#include <asm/byteorder.h>
#include <stdint.h>

struct dtp_hdr
{
    __u32 fid;     /* flow ID */
    __u32 seq;     /* sequence number */
    __u32 ack_seq; /* acknowledgement number */
#if defined(__LITTLE_ENDIAN_BITFIELD)
	__u16 doff:4,  /* data offset */
		mobile:1,  /* mobile */
        res:2,     /* reserved */
        chg:1,     /* challenge */
        
		rsp:1,     /* response */
        auth:1,    /* authenticate */
		urg:1,     /* urgent */
		ack:1,     /* ack */
		psh:1,     /* push */
		rst:1,     /* reset */
		syn:1,     /* syn */
		fin:1;     /* fin */
#elif defined(__BIG_ENDIAN_BITFIELD)
	__u16 chg:1,
        res:2,
		mobile:1,
        doff:4,
        fin:1,
        syn:1,
        rst:1,
        psh:1,
		ack:1,
		urg:1,
		auth:1,
		rsp:1;
#else
#error "Check the definition in <asm/byteorder.h>"
#endif
    __u16 window_size;    /* window size */
    __u16 checksum;       /* checksum */
    __u16 urgent_pointer; /* urgent pointer */
}__attribute__ ((packed));

/*
 *    DTP header OPTION field  -- under the task, it can be modified.
 */

/* Opcode representing each option field */
#define DTPOPT_EOL             0  /* End of the option list */
#define DTPOPT_NOP             1  /* No operation */
#define DTPOPT_MSS             2  /* Maximum Segment Size */
#define DTPOPT_WIN_SCALE       3  /* Window Scale Factor */
#define DTPOPT_SACK_PERM       4  /* SACK permitted */
#define DTPOPT_SACK            5  /* SACK */
#define DTPOPT_HOST_ID         7  /* Identifier for the host */
#define DTPOPT_PORT            8  /* New port notification */
#define DTPOPT_DEADLINE        9  /* DTP deadline */

/* Length for each option fields (including opcode and oplen field size) */
#define DTPOLEN_MSS            4  /* 2 + 2 */
#define DTPOLEN_WIN_SCALE      3  /* 2 + 1 */
#define DTPOLEN_SACK_PERM      2  /* 2 + 0 */
#define DTPOLEN_HOST_ID        22 /* 2 + 20 */
#define DTPOLEN_PORT           4  /* 2 + 2 */
#define DTPOLEN_DEADLINE       6  /* 2 + 4 */

/* Default number of selective ACK blocks*/
#define DTP_NUM_SACKS          4
/* Default length of a selective ACK block*/
#define DTPOLEN_SACK_BASE      1  /* SACK BASE : tells about num of SACK blocks */
#define DTPOLEN_SACK_PERBLOCK  8  /* starting SEQ (4 byte) + end SEQ (4 byte) */

/* flag */
#define DTP_FLAG_MOBILE        0x80
#define DTP_FLAG_CHG           0x40
#define DTP_FLAG_RSP           0x20
#define DTP_FLAG_AUTH          0x10
#define DTP_FLAG_ACK           0x08
#define DTP_FLAG_RST           0x04
#define DTP_FLAG_SYN           0x02
#define DTP_FLAG_FIN           0x01


extern uint32_t DTPGetRandomNumber(void);

/*
 *  DTPGenerateHostID() - Generate a new host ID.
 *
 *  hostID: pointer to hostID that have to be filled up.
 */
extern void DTPGenerateHostID(u_char* hostID);

/*
 *  DTPGenerateFlowID() - Generate a new flow ID.
 *
 *  hostID: pointer to hostID, which is used for generating flow ID.
 */
extern uint32_t DTPGenerateFlowID(const u_char* hostID);

extern int DTPGenerateHeader(u_char* p, uint32_t seq_num, uint32_t ack_seq_num,
			    uint32_t flowid, int flag, uint32_t win_size);

extern int DTPAddOptionToHeader(u_char *p, int offset, uint8_t opcode,
			       void* op_ptr);

extern void DTPPrintPacket(const u_char* p, int totalLen);

extern int DTPGetOption(u_char* buf, u_char *host_id, uint32_t *keep_alive);

extern void DTPParseOption(u_char* buf, const void* this_ctx, int isSYNPacket);


#ifndef __min
#define __min(x, y) (((x) > (y)) ? (y) : (x))
#endif

/* buffer struct */
typedef struct buffer
{
	uint32_t seq;
	uint32_t len;
	struct buffer *next_buf;
	TAILQ_ENTRY(buffer) link;
} buffer;

/* buffer list */
buffer *GetBuffer(void);
void AddToFreeBufferList(buffer *b);
void FreeBufferLockInit();

#endif
