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

#define SCHED_SOCK 12321
#define LOCALHOST  "127.0.0.1"
#define BUFSIZE    1024

enum {REQUEST_NOT_FOUND = -1, REQUEST_MALFORMED = -2, 
	  REQUEST_AGAIN = -3};

int g_sched_sock = -1;


#ifdef HAVE_SCHEDULER
/*-------------------------------------------------------------------*/
int 
ConnectScheduler(void)
{
	int sock;
	struct sockaddr_in sched_addr;
	
	/* create socket */
	if ((sock = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
		LOGD("socket() error\n");
		return -1;
    }

	LOGD("ConnectScheduler : created socket\n");

	/* set scheduler address */
	memset(&sched_addr, 0, sizeof(sched_addr));
	sched_addr.sin_family = AF_INET;
	sched_addr.sin_addr.s_addr = inet_addr(LOCALHOST);
	sched_addr.sin_port = htons(SCHED_SOCK);

	/* set sock to non-block */
	
	if (fcntl(sock, F_SETFL, O_NONBLOCK) == -1) {
		LOGD("fcntl() error");
		return -1;
	}
	
	LOGD("ConnectScheduler : fcntl\n");

	/* connect to scheduler */
	if (connect(sock, (const struct sockaddr*)&sched_addr, 
				sizeof(sched_addr)) == -1) {

		LOGD("ConnectScheduler : connect returned -1\n");
		LOGD("ConnectScheduler : errno = %d\n", errno);

		if (errno == EINPROGRESS) {
			LOGD("ConnectScheduler : EINPROGRESS, returning %d\n", sock);
			g_sched_sock = sock;
			return sock;
		}
		else {
			LOGD("connect() error\n");
			close(sock);
			return -1;
		}
	}	

	LOGD("ConnectScheduler : after connect success\n");

	g_sched_sock = sock;

	return sock;
}
/*-------------------------------------------------------------------*/
int
SendSchedulerMessage()
{
    dtp_context *ctx;
	int count = 0;
    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
		if (ctx->tc_state == SOCK_ESTABLISHED)
			count++;
	}
	if (count == 0) {
		LOGD("COUNT == 0, returning");
		return 0;
	}

	char message[BUFSIZE];
	int res = 0, len;
	int optval = -1;
	int optval_len = sizeof(optval);

	sprintf(message, "%d,100000", count);

	struct timeval tv;
	double now, diff;
    if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, return -1);
    }
    now = tv.tv_sec + (tv.tv_usec / 1e6);

    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {

		/* in case ConnectScheduler() failed before */
		/*
		  if (*sock == -1) {
		  LOGD("ConnectScheduler() start\n");
		  if ((*sock = ConnectScheduler()) == -1) {
		  LOGD("ConnectScheduler() error: can't connect to scheduler\n");
		  return -1;
		  }
		  LOGD("ConnectScheduler() returned\n");
		  }
		*/

		if (ctx->tc_state == SOCK_ESTABLISHED) {

			if (getsockopt(g_sched_sock, SOL_SOCKET, SO_ERROR, (int *)&optval, 
						   (socklen_t *)&optval_len) == 0) {
				
				diff = ctx->tc_deadlineTime.tv_sec + 
					(ctx->tc_deadlineTime.tv_usec / 1e6) - now;
				
				/* generate message */
				if ((len = sprintf(message + strlen(message), ",%u,%d,%u,%d", 
								   ctx->tc_flowID, 
								   ctx->tc_schedLink,
								   (ctx->tc_isDeadlineSet)? ctx->tc_blockRemain : 0,
								   (ctx->tc_isDeadlineSet)? (int)diff : 0)) < 0) {
					LOGD("sprintf() error\n");
					return -1;
				}
			}
		}
	}

	sprintf(message + strlen(message), "\n");

	LOGD("message : %s", message);
			
	/* send message to scheduler */
	if ((res = write(g_sched_sock, message, strlen(message))) == -1) {
		if (errno == EAGAIN) {
			LOGD("failed to send message to scheduler\n");
			return REQUEST_AGAIN;
		}
		else if (errno == EPIPE) {
				LOGD("scheduler is dead\n");
				close(g_sched_sock);
				g_sched_sock = ConnectScheduler();

				// create scheduler listen event
				struct event *sched_event;
				if (g_sched_sock > 0) {
	
					/* create sheduler read event */
					sched_event = event_new(GetEventBase(), g_sched_sock, EV_READ|EV_PERSIST,
											DTPSchedEvent, NULL);
					if (!sched_event) {
						LOGD("event_new failed\n");
						EXIT(-1, return -1);
					}
					
					res = event_add(sched_event, NULL);
					if (res < 0) {
						TRACE("event_add failed\n");
						EXIT(-1, return -1);		
					}
				}
				return -2;
		}
		else {
			LOGD("write() error\n");
		}
	}

	//	LOGD("write() end : len : %d, res = %d / SendSchedulerMessage() finished\n"
	//		 , strlen(message), res);
	return res;
}
/*-------------------------------------------------------------------*/
/*
  int
  DTPCloseScheduler(dtp_context *ctx)
  {
  int res;

  ctx->tc_schedStatus = FLOW_END;

  if ((res = SendSchedulerMessage(&ctx->tc_schedSock, 
  ctx->tc_schedStatus,
  ctx->tc_schedLink, 
  ctx->tc_blockRemain, 
  ctx->tc_deadline)) == -1) {
  LOGD("failed to send message to scheduler\n");
  g_useScheduler = FALSE;
  gettimeofday(&ctx->tc_schedTime, NULL);
  }
  else if (res == -2) {
  g_useScheduler = FALSE;
  gettimeofday(&ctx->tc_schedTime, NULL);
  }
  else {
  LOGD("must resend message again\n");
  return -1;
  }
  close(ctx->tc_schedSock);

  res = event_del(ctx->tc_schedEvent);
  if (res != 0) {
  LOGD("event_del failed\n");
  exit(-1);
  }

  return 0;
  }
*/

static void
OnSchedEvent(int sock)
{
	dtp_context *ctx;
	char message[BUFSIZE];
	int res;

	/* read message */
	if ((res = read(sock, message, BUFSIZE)) == -1) {
		LOGD("scheduler closed the socket. stop using scheduler\n");
		if (errno != EAGAIN) {
			
		}
	}			
	else if (res == 0) {
		/* XXX : SHOULD BE HANDLED */
				LOGD("scheduler closed the socket. stop using scheduler\n");
		//		g_useScheduler = FALSE;
	}
	else {
		char *flowid, *result;
		uint32_t a;
		int b;		
		dtp_context *ctx;

		result = NULL;
		if ((flowid = strtok(message, ",")) != NULL)
			result = strtok(NULL, ",");

		while (result != NULL) {
			a = atoi(flowid);
			
			LOGD("* %u (%s)", a, result);

			TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {				
				if (ctx->tc_flowID == a) {
					ctx->tc_scheduleSend = (result[0] == '1')? TRUE : FALSE;
					if (IsAvailConnection(ctx)) {
						DTPSendACKPacket(ctx);
						ctx->tc_schedAllowTime += SCHED_TIMEOUT;
					}
				}
			}

			result = NULL;
			if ((flowid = strtok(NULL, ",")) != NULL)
				result = strtok(NULL, ",");
		}

		/* XXX : SCHEDULE_SEND SHOULD BE MODIFIED -- PARSE AND FIND EACH FLOWID */
		//	ctx = DTPGetContextByFlowId(fid);
		// ctx->tc_scheduleSend = (message[0] == '0') ? FALSE : TRUE;
	}
}
/*-------------------------------------------------------------------*/
void
DTPSchedEvent(int sock, short event, void *arg)
{
	/* message received from scheduler */
	if (event & EV_READ) 
		OnSchedEvent(sock);
}


#endif
