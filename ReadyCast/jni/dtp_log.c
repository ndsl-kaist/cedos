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
#include "dhkim_debug.h"
#ifdef IN_MOBILE
#include "scheduler.h"
#endif

/*-------------------------------------------------------------------*/
int
SetTime(dtp_socket_t socket, struct timeval *tv)
{
    dtp_context *ctx = NULL;
	
    ctx = DTPGetContextBySocket(socket);
    if (ctx) {
		if (pthread_mutex_lock(&ctx->tc_connTimeLock)) {
			TRACE_ERR("pthread_mutex_lock() failed");
			EXIT(-1, return -1);
		}
		if (ctx->tc_isClientMobile) {
			ctx->tc_mobileTime = (tv->tv_sec * 1000 + tv->tv_usec / 1000) - 
				(ctx->tc_startTime.tv_sec * 1000 + ctx->tc_startTime.tv_usec / 1000);
			ctx->tc_wifiTime = 0;
		}
		else {
			ctx->tc_mobileTime = 0;
			ctx->tc_wifiTime = (tv->tv_sec * 1000 + tv->tv_usec / 1000) - 
				(ctx->tc_startTime.tv_sec * 1000 + ctx->tc_startTime.tv_usec / 1000);
		}
		if (pthread_mutex_unlock(&ctx->tc_connTimeLock)) {
			TRACE_ERR("pthread_mutex_unlock() failed");
			EXIT(-1, );
		}
		return 0;
	}
    else 
		TRACE("context doesn't exist\n");
	
	return -1;
}
/*-------------------------------------------------------------------*/
int
SetUpByteCount(dtp_socket_t socket, int byte)
{
    dtp_context *ctx = NULL;
	
    ctx = DTPGetContextBySocket(socket);
    if (ctx) {
		if (pthread_mutex_lock(&ctx->tc_upByteLock)) {
			TRACE_ERR("pthread_mutex_lock() failed");
			EXIT(-1, return -1);
		}
		ctx->tc_upByte = byte;
		if (pthread_mutex_unlock(&ctx->tc_upByteLock)) {
			TRACE_ERR("pthread_mutex_unlock() failed");
			EXIT(-1, );
		}
		return 0;
	}
    else 
		TRACE("context doesn't exist\n");

	return -1;
}
/*-------------------------------------------------------------------*/
int
SetDownByteCount(dtp_socket_t socket, int byte)
{
    dtp_context *ctx = NULL;
	
    ctx = DTPGetContextBySocket(socket);
    if (ctx) {
		if (pthread_mutex_lock(&ctx->tc_downByteLock)) {
			TRACE_ERR("pthread_mutex_lock() failed");
			EXIT(-1, return 0);
		}
		ctx->tc_downByte = byte;
		if (pthread_mutex_unlock(&ctx->tc_downByteLock)) {
			TRACE_ERR("pthread_mutex_unlock() failed");
			EXIT(-1, );
		}
		return 0;
	}
    else 
		TRACE("context doesn't exist\n");

	return -1;
}
/*-------------------------------------------------------------------*/
uint32_t 
dtp_getflowid(dtp_socket_t socket)
{
    dtp_context *ctx = NULL;

    ctx = DTPGetContextBySocket(socket);
    if (ctx)
		return ctx->tc_flowID;
    else 
		TRACE("context doesn't exist\n");

    return -1;
}
/*-------------------------------------------------------------------*/
u_char *
dtp_gethostid(dtp_socket_t socket)
{
    dtp_context *ctx = NULL;

    ctx = DTPGetContextBySocket(socket);
    if (ctx)
		return ctx->tc_hostID;
    else 
		TRACE("context doesn't exist\n");

    return NULL;
}
/*-------------------------------------------------------------------*/
int
dtp_getsocklog(dtp_socket_t socket, int level, int optname,
			   void *optval, socklen_t *optlen)
{
    dtp_context* ctx;
    ctx = DTPGetContextBySocket(socket);

    if (!ctx->tc_isActive) {
		errno = EBADF;
		return -1;
    }

    switch (optname) {
	case DTP_UPBYTE:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_upByte), *optlen);
		break;
		
	case DTP_DOWNBYTE:
		*optlen = sizeof(int);
		memcpy(optval, &(ctx->tc_downByte), *optlen);
		break;

	case DTP_MOBILETIME:
		*optlen = sizeof(long long int);
		memcpy(optval, &(ctx->tc_mobileTime), *optlen);
		break;

	case DTP_WIFITIME:
		*optlen = sizeof(long long int);
		memcpy(optval, &(ctx->tc_wifiTime), *optlen);
		break;

    default:
		TRACE("Currently, dtp_getsockopt() supports only DTP_SO_ACCEPTCONN, DTP_SO_RCVBUF, DTP_SO_SNDBUF.\n");
		errno = ENOPROTOOPT;
		return -1;
    }

    return 0;
}
/*-------------------------------------------------------------------*/
