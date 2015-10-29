#ifndef __DTP_RETRANS_H__
#define __DTP_RETRANS_H__

#include "queue.h"
#include "dtp.h"

static TAILQ_HEAD(tailQ, dtp_context) g_retTimerQHead = 
    TAILQ_HEAD_INITIALIZER(g_retTimerQHead);

extern pthread_mutex_t* GetRetransQueueLock();
extern struct tailQ* GetRetransQueue();

extern void RetransQueueLockInit();
extern void InsertToRetransQueue(dtp_context* new_ctx);
extern void RemoveFromRetransQueue(dtp_context* ctx);

#endif
