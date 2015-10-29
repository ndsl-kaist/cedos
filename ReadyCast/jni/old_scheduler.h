#ifndef __SCHEDULER_H__
#define __SCHEDULER_H__

/* flow termination flag */
#define FLOW_INPROGRESS  0
#define FLOW_END         1

/* donw/up link flag */
#define LINK_DOWN        0
#define LINK_UP          1

/* scheduler timeout */
#define SCHED_TIMEOUT   20  /* in seconds */

/*
 *  SendSchedulerMessage() : send message to scheduler
 *                           return length of message
 */
int SendSchedulerMessage(void);

/* 
 *  ConnectSchduler() : connect to scheduler
 *                      return TCP socket number
 */
int ConnectScheduler(void);

/* 
 *  CloseScheduler() : Close the scheduler connection
 */
int DTPCloseScheduler(dtp_context *ctx);

void DTPSchedEvent(int sock, short event, void *arg);

#endif
