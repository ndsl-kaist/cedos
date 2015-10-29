#define _GNU_SOURCE
#include <stdio.h>
#include <stdbool.h>
//#include <sys/types.h>
#include <assert.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/engine.h>
#include <openssl/hmac.h>
#include <netinet/in.h>
#include "dtp_transport.h"
#include "dtp_select.h"
#include "debug.h"

#include "dhkim_debug.h"

/* socketpair used to send event to dtp_socket thread */
static int inner_sock;

/* parameters passed by dtp_select() */
static int g_nfds;
static fd_set_s* g_select_rfds;
static fd_set_s* g_select_wfds;

/* true if dtp_select() waiting */
static bool g_isselectWaiting = false;

/* monitors the events */
static fd_set_s g_fds;   /* true if corresponding dtp socket exists  */
static fd_set_s g_rfds;  /* true if corresponding dtp socket has read data */
static fd_set_s g_wfds;  /* true if corresponding dtp socket has write data */

/*-------------------------------------------------------------------*/
void
DTPSetInnerSock(int sock)
{
    inner_sock = sock;

    /* initialize g_rfds, g_wfds */
    FD_ZERO_S(&g_rfds);
    FD_ZERO_S(&g_wfds);
}
/*-------------------------------------------------------------------*/
fd_set_s*
DTPGetFdSet()
{
	return &g_fds;
}
/*-------------------------------------------------------------------*/
fd_set_s*
DTPGetFdRSet()
{
	return &g_rfds;
}
/*-------------------------------------------------------------------*/
bool
DTPIsDTPSocket(int sock)
{
    return FD_ISSET_S(sock, &g_fds);
}

/*-------------------------------------------------------------------*/
void
DTPRegisterSockToGlobalFdSet(int sock)
{
	/*
	if (sock > 1023)
		DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"/error.txt",
				"Trying to register sock %d", sock);
				*/
    FD_SET_S(sock, &g_fds);  /* mark new socket number for dtp */
}
/*-------------------------------------------------------------------*/
void
DTPClearSockFromGlobalFdSet(int sock)
{
    FD_CLR_S(sock, &g_fds);  /* clear the dtp socket number */
}
/*-------------------------------------------------------------------*/
int
DTPSelectCheckAnyPendingEvent(int nfds, fd_set_s* readfds, fd_set_s* writefds)
{
    int i;
    int count = 0;

    if (readfds) {
		for (i = 0; i < nfds; i++)
			if (FD_ISSET_S(i, readfds)) {
				if (FD_ISSET_S(i, &g_rfds)) /* if there is any pending event */
					count++;
				else                      /* if there is no pending event */
					FD_CLR_S(i, readfds);
			}
    }

    if (writefds) {
		for (i = 0; i < nfds; i++)
			if (FD_ISSET_S(i, writefds)) {
				if (FD_ISSET_S(i, &g_wfds)) /* if there is any pending event */
					count++;
				else                      /* if there is no pending event */
					FD_CLR_S(i, writefds);
			}
    }
    
    return count;
}
/*-------------------------------------------------------------------*/
void
DTPSelectInit(int nfds, fd_set_s* readfds, fd_set_s* writefds)
{
    g_nfds = nfds;
    g_select_rfds = readfds;
    g_select_wfds = writefds;
    g_isselectWaiting = true;
}

/*-------------------------------------------------------------------*/
void
DTPSelectClear()
{
    g_isselectWaiting = false;
}
/*-------------------------------------------------------------------*/
void
DTPSelectEventSet(int sock, int rw)
{
    dtp_event ev;

    if (rw == DTP_FD_READ) {
		FD_SET_S(sock, &g_rfds);
		if (sock < g_nfds) {
			if (g_isselectWaiting && FD_ISSET_S(sock, g_select_rfds)) {
				g_isselectWaiting = false;
				ev.te_fd = sock;
				ev.te_command = DTP_FD_READ;
				DTPSendEventToLibThread(inner_sock, &ev);
			}
		}
    }
    else /* if (rw == DTP_FD_WRITE) */ {
		FD_SET_S(sock, &g_wfds);
		if (sock < g_nfds) {
			if (g_isselectWaiting && FD_ISSET_S(sock, g_select_wfds)) {
				g_isselectWaiting = false;
				ev.te_fd = sock;
				ev.te_command = DTP_FD_WRITE;
				DTPSendEventToLibThread(inner_sock, &ev);
			}
		}
    }
}
/*-------------------------------------------------------------------*/
void
DTPSelectEventClr(int sock, int rw)
{
    if (rw == DTP_FD_READ) {
		FD_CLR_S(sock, &g_rfds);
    }
    else /* if (rw == DTP_FD_WRITE) */ {
    	FD_CLR_S(sock, &g_wfds);
    }
}

