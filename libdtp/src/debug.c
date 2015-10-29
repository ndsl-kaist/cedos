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
#include <net/if.h>
#include <netinet/in.h>
#include "dtp_transport.h"
#include "debug.h"
#include "dtp.h"
static double timer;

/*--------------------------------------------------------------------*/
void
DTPPrintPacket(const u_char* p, int totalLen)
{
    struct dtp_hdr *hdr = (struct dtp_hdr *)p;
    int hdrLen = hdr->doff << 2;
    struct timeval tv;

    fprintf(stderr, "------------------ Packet Start -------------------\n");
    fprintf(stderr, "hdr_size: %d (opt_size: %d) / data_size: %d\n", 
			hdrLen, hdrLen - (int)sizeof(*hdr), totalLen - hdrLen);
    fprintf(stderr, "seq: %u / ack: %u / flowid: %08X / win size: %u flags: ", 
			ntohl(hdr->seq), ntohl(hdr->ack_seq), 
			ntohl(hdr->fid), ntohs(hdr->window_size));
    if (hdr->syn == 1) fprintf(stderr, "SYN ");
    if (hdr->ack == 1) fprintf(stderr, "ACK ");
    if (hdr->fin == 1) fprintf(stderr, "FIN ");
    if (hdr->rst == 1) fprintf(stderr, "RST ");
    if (hdr->chg == 1) fprintf(stderr, "CHG ");
    if (hdr->rsp == 1) fprintf(stderr, "RSP ");
    if (hdr->auth == 1) fprintf(stderr, "AUTH ");
	gettimeofday(&tv, NULL);
	fprintf(stderr, "\n[pkt_time] %f", (tv.tv_sec + (tv.tv_usec)/1e6));
    fprintf(stderr, "\n------------------ Packet End -------------------\n");

#ifdef IN_MOBILE
	/*
    LOGD("------------------ Packet Start -------------------\n");
    LOGD("hdr_size: %d (opt_size: %d) / data_size: %d\n", 
			hdrLen, hdrLen - (int)sizeof(*hdr), totalLen - hdrLen);
    LOGD("seq: %u / ack: %u / flowid: %08X / win size: %u flags: ", 
			ntohl(hdr->seq), ntohl(hdr->ack_seq), 
			ntohl(hdr->fid), ntohs(hdr->window_size));
    if (hdr->syn == 1) LOGD("SYN ");
    if (hdr->ack == 1) LOGD("ACK ");
    if (hdr->fin == 1) LOGD("FIN ");
    if (hdr->rst == 1) LOGD("RST ");
    if (hdr->chg == 1) LOGD("CHG ");
    if (hdr->rsp == 1) LOGD("RSP ");
    if (hdr->auth == 1) LOGD("AUTH ");
	gettimeofday(&tv, NULL);
	LOGD("\n[pkt_time] %f", (tv.tv_sec + (tv.tv_usec)/1e6));
    LOGD("\n------------------ Packet End -------------------\n");
	*/
#endif
}

/*--------------------------------------------------------------------*/
void
DTPPrintTimestamp(char* c) {
    struct timeval tv;
	gettimeofday(&tv, NULL);
	fprintf(stderr, "[%s] %f\n", c, (tv.tv_sec + (tv.tv_usec)/1e6));
}

/*--------------------------------------------------------------------*/
void
DTPTimerStart() {
    struct timeval tv;
	gettimeofday(&tv, NULL);
	timer = tv.tv_sec + (tv.tv_usec)/1e6;
}
/*--------------------------------------------------------------------*/
void
DTPTimerEnd(char *c) {
    struct timeval tv;
	gettimeofday(&tv, NULL);
	fprintf(stderr, "[%s] %f\n", c, (tv.tv_sec + (tv.tv_usec)/1e6) - timer);
}
