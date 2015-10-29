#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include "context.h"
#include "queue.h"
#include "debug.h"
#include "dtp_retrans_queue.h"

#include "dhkim_debug.h"

static pthread_mutex_t g_timerLock;  /* timer lock */

/*-------------------------------------------------------------------*/
pthread_mutex_t* GetRetransQueueLock() {
	return &g_timerLock;  /* timer lock */
}
/*-------------------------------------------------------------------*/
struct tailQ* GetRetransQueue() {
	return &g_retTimerQHead;
}
/*-------------------------------------------------------------------*/
void
RetransQueueLockInit() {
    if (pthread_mutex_init(&g_timerLock, NULL)) {
		TRACE_ERR("pthread_mutex_init() failed\n");
		EXIT(-1, return);
    }
}

/*-------------------------------------------------------------------*/
void
InsertToRetransQueue(dtp_context* new_ctx) {
	LOGD("InsertToRetransQueue");
	dtp_context* item;

	/* add context to g_retTimerQHead queue */
    if (pthread_mutex_lock(&g_timerLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }

	// ordered insert by deadline
	int inserted = 0;
	TAILQ_FOREACH(item, &g_retTimerQHead, tc_link) {
		if (item->tc_deadlineTime.tv_sec > new_ctx->tc_deadlineTime.tv_sec) {
			LOGD("Insert BEFORE D = %d, %d\n", new_ctx->tc_deadline, new_ctx->tc_deadlineTime.tv_sec);
			TAILQ_INSERT_BEFORE(/*&g_retTimerQHead,*/ item, new_ctx,
								tc_link);
			inserted = 1;
			break;
		}
	}
	if (!inserted) {
		LOGD("Insert TAIL = %d, %d\n", new_ctx->tc_deadline, new_ctx->tc_deadlineTime.tv_sec);
		TAILQ_INSERT_TAIL(&g_retTimerQHead, new_ctx, tc_link);
	}

    if (pthread_mutex_unlock(&g_timerLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }
}

/*-------------------------------------------------------------------*/
void
RemoveFromRetransQueue(dtp_context* ctx) {
	LOGD("RemoveFromRetransQueue");

	/* remove context from g_retTimerQHead queue */
	if (pthread_mutex_lock(&g_timerLock)) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
	}
	TAILQ_REMOVE(&g_retTimerQHead, ctx, tc_link);
	if (pthread_mutex_unlock(&g_timerLock)) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
	}
}
/*-------------------------------------------------------------------*/
