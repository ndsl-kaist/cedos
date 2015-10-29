#include <stdlib.h>
#include <assert.h>
#include <pthread.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include "debug.h"
#include "context.h"

#include "dhkim_debug.h"

/* total # of lsocks:
   NUM_BATCH * BATCH_SIZE = 4M */
#define NUM_BATCH       1024
#ifdef IN_MOBILE
#define BATCH_SIZE_BITS 8
#else
#define BATCH_SIZE_BITS 12
#endif
#define BATCH_SIZE      (1 << BATCH_SIZE_BITS)

/* logical socket array */
static dtp_context *g_pcontext[NUM_BATCH] = {0}; /* init to NULL */

/* flow id hash table bins */
#define NUM_FID_MAP_BINS 65536
#define HASH_TO_FIDMAP_BIN(x) ((x) & (NUM_FID_MAP_BINS - 1))
static TAILQ_HEAD(FlowIDMap, dtp_context) g_FlowIDMapBins[NUM_FID_MAP_BINS];

/*-------------------------------------------------------------------*/
dtp_context*
DTPAllocateContext(int sock)
{
    /* allocate the context if empty */
    const int bat = (sock >> BATCH_SIZE_BITS);
    const int idx = (sock & (BATCH_SIZE-1));

    if (bat >= NUM_BATCH) {
		TRACE("we're out of batch!\n");
		return NULL;
    }

    if (g_pcontext[bat] == NULL) {
		g_pcontext[bat] = calloc(BATCH_SIZE, sizeof(dtp_context));
		if (g_pcontext[bat] == NULL) {
			TRACE("allocating another context batch failed\n");
			return NULL;
		}
    }
    return (&(g_pcontext[bat][idx]));
}
/*-------------------------------------------------------------------*/
dtp_context *
DTPGetContextBySocket(int sock)
{
    const int bat = (sock >> BATCH_SIZE_BITS);
    const int idx = (sock & (BATCH_SIZE-1));

    if (sock < 0 || bat >= NUM_BATCH) {
		/* should not happen */
		ASSERT(0, ); 
		return (NULL);
    }
    return (&(g_pcontext[bat][idx]));
}
/*-------------------------------------------------------------------*/
void 
InitializeFlowIDMap(void)
{
    int i;
    for (i = 0; i < NUM_FID_MAP_BINS; i++)
		TAILQ_INIT(&g_FlowIDMapBins[i]);
}
/*-------------------------------------------------------------------*/
void
DTPAddContextToFlowIDMap(uint32_t fid, dtp_context* pctx) 
{
    int idx = HASH_TO_FIDMAP_BIN(fid);
    TAILQ_INSERT_HEAD(&g_FlowIDMapBins[idx], pctx, tc_flowIDMapLink);
    pctx->tc_onFlowIDMap = TRUE;
}
/*-------------------------------------------------------------------*/
void
DTPRemoveContextFromFlowIDMap(dtp_context *pctx)
{
    if (pctx->tc_onFlowIDMap) {
		int idx = HASH_TO_FIDMAP_BIN(pctx->tc_flowID);
		TAILQ_REMOVE(&g_FlowIDMapBins[idx], pctx, tc_flowIDMapLink);
		pctx->tc_onFlowIDMap = FALSE;
    }
}
/*-------------------------------------------------------------------*/
dtp_context *
DTPGetContextByFlowID(uint32_t fid) 
{
    dtp_context *pctx;
    int idx = HASH_TO_FIDMAP_BIN(fid);
	
    TAILQ_FOREACH(pctx, &g_FlowIDMapBins[idx], tc_flowIDMapLink) {
		if (pctx->tc_flowID == fid)
			return (pctx);
    }
    return(NULL); 
}

/* retransmission timer queue */
static TAILQ_HEAD(, dtp_pkt) g_freePacketList = TAILQ_HEAD_INITIALIZER(g_freePacketList);
static int g_freePktCount = 0;
static pthread_mutex_t g_pktLock;  /* packet array lock */

/*-------------------------------------------------------------------*/
static void
DoAddToFreePacketList(dtp_pkt *p)
{
    TAILQ_INSERT_HEAD(&g_freePacketList, p, tp_freeLink);
    g_freePktCount++;
}
/*-------------------------------------------------------------------*/
static void
AllocateMorePackets(void)
{
    dtp_pkt *p;
    int i;
    
    for (i = 0; i < BATCH_SIZE; i++) {
		if ((p = calloc(1, sizeof(dtp_pkt))) == NULL) {
			TRACE_ERR("callocating packet failed\n");
			EXIT(-1, break);
		}
		DoAddToFreePacketList(p);
    }
}
/*-------------------------------------------------------------------*/
void
AddToFreePacketList(dtp_pkt *p)
{
    if (pthread_mutex_lock(&g_pktLock)) {
		perror("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
    DoAddToFreePacketList(p);
    if (pthread_mutex_unlock(&g_pktLock)) {
		perror("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }
}
/*-------------------------------------------------------------------*/
static dtp_pkt *
RemoveFromFreePacketList(void)
{
    dtp_pkt *p;

    /* acquire a lock to access the shared variables */
    if (pthread_mutex_lock(&g_pktLock)) {
		perror("pthread_mutex_lock() failed");
		EXIT(-1, return NULL);
    }
    
    p = TAILQ_FIRST(&g_freePacketList);
    if (p == NULL) {
		AllocateMorePackets();
		p = TAILQ_FIRST(&g_freePacketList);
    }
    TAILQ_REMOVE(&g_freePacketList, p, tp_freeLink);
    g_freePktCount--;
    
    if (pthread_mutex_unlock(&g_pktLock)) {
		perror("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }
    return(p);
}
/*-------------------------------------------------------------------*/
void
DTPInitializeIdlePacket(void)
{
    dtp_pkt *p;

    /* initialize packet array lock */
    if (pthread_mutex_init(&g_pktLock, NULL)) {
		TRACE_ERR("pthread_mutex_init() failed\n");
		EXIT(-1, return);
    }
    p = RemoveFromFreePacketList();
    AddToFreePacketList(p);
}
/*-------------------------------------------------------------------*/
dtp_pkt*
DTPGetIdlePacket(void) 
{
    return RemoveFromFreePacketList();
}

