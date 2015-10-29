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
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include "dtp_select.h"
#include "dtp_socket.h"
#include "dtp_transport.h"
#include "debug.h"
#include "context.h"
#include "scheduler.h"
#include "dtp_retrans_queue.h"
#include "queue.h"
#include "dhkim_debug.h"

void
Schedule(double wSpeed, double cSpeed) {
	LOGD("WiFi : %lf / Cell : %lf", wSpeed, cSpeed);

	dtp_context* ctx;

	/*
	int count = 0;
    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
		ctx->tc_scheduleSend = (count == 0); // EDF with no DTN
		count++;
	}
	*/

	struct timeval tv;
	double T_EDF = 0.0, now;
	if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, );
	}
	now = tv.tv_sec + (tv.tv_usec / 1e6);


	TAILQ_FOREACH_REVERSE(ctx, GetRetransQueue(), tailQ, tc_link) {
		if (T_EDF == 0) {
			T_EDF = (ctx->tc_deadlineTime.tv_sec + ctx->tc_deadlineTime.tv_usec / 1e6 + 1)
				- (((double)ctx->tc_blockRemain * 8.0) / (cSpeed * 1000.0 * 1000.0)) ;
		}
		else {
			T_EDF = MIN(T_EDF, (ctx->tc_deadlineTime.tv_sec
								+ ctx->tc_deadlineTime.tv_usec / 1e6 + 1))
				- ((double)(ctx->tc_blockRemain * 8.0)) / (cSpeed * 1000.0 * 1000.0) ;
		}
		LOGD("[Schedule] %d / timeUntilDeadline : %.2f / ttT_EDF : %lf / state : %d\n",
			 ctx->tc_sock,
			 ctx->tc_deadlineTime.tv_sec + ctx->tc_deadlineTime.tv_usec / 1e6 - now,
			 T_EDF - now,
			 ctx->tc_state);
		ctx->tc_scheduleSend = FALSE;
	}	

	if ((ctx = TAILQ_FIRST(GetRetransQueue())) != NULL) {
		if (IsMobileConnected()) {	
			if (T_EDF - now <= 0) {
				ctx->tc_scheduleSend = TRUE;
				LOGD("[Schedule] ALLOW to send %d", ctx->tc_sock);
			}
			else {
				ctx->tc_scheduleSend = FALSE;
				LOGD("[Schedule] DENY to send %d", ctx->tc_sock);
			}
		}
		else {
			ctx->tc_scheduleSend = TRUE;
			LOGD("[Schedule] ALLOW to send %d", ctx->tc_sock);			
			if ((cSpeed > wSpeed) && (T_EDF - now <= 0)) {
				SetMobileToUse();
			}
			else {
				SetWiFiToUse();
			}
		}
	}

	/*
    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
		struct timeval tv; double timeUntilDeadline;
        if (gettimeofday(&tv, NULL)) {
            perror("gettimeofday() failed");
			EXIT(-1, );
        }
        timeUntilDeadline = ctx->tc_deadlineTime.tv_sec - tv.tv_sec
			+ (ctx->tc_deadlineTime.tv_usec / 1e6) - (tv.tv_usec / 1e6);
		LOGD("[Schedule] %d / timeUntilDeadline : %.2f / allowed : %d / state : %d\n",
			 ctx->tc_sock, timeUntilDeadline, ctx->tc_scheduleSend, ctx->tc_state);
	}
	*/

}
