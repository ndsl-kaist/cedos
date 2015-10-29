#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdint.h>
#include "dtp_transport.h"

dtp_context* DTPAllocateContext(int sock);
dtp_context *DTPGetContextBySocket(int sock);

/* flow ID hash table */
void InitializeFlowIDMap(void);
void DTPAddContextToFlowIDMap(uint32_t fid, dtp_context* pctx);
void DTPRemoveContextFromFlowIDMap(dtp_context *pctx);
dtp_context *DTPGetContextByFlowID(uint32_t fid);

dtp_pkt* DTPGetIdlePacket(void);
void DTPInitializeIdlePacket(void);
void AddToFreePacketList(dtp_pkt *p);

#endif // _CONTEXT_H_
