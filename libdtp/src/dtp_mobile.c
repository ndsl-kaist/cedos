#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <event2/event.h>
#include <math.h>
#include <signal.h>
#include <openssl/hmac.h>
#include "dtp_transport.h"
#include "dtp_select.h"
#include "dtp_mobile.h"
#include "dtp.h"
#include "crypt.h"
#include "debug.h"
#include "context.h"
#include "dtp_retrans_queue.h"
#include "dhkim_debug.h"
#include "dtp_socket.h"

#include <netinet/in.h>
#include <net/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

#ifdef IN_MOBILE
#include "ifaddrs.h"

#define DEADLINE_LAST_TIMEOUT 3*60

static int networkTypeLast = 0;
static uint32_t ipaddrLast = 0;

int
GetNetworkTypeLast() {
	return networkTypeLast;
}

uint32_t
GetIpaddrLast() {
	return ipaddrLast;
}

/*-------------------------------------------------------------------*/
bool
IsMobileConnected()
{
	return g_mobileConnected;
}
/*-------------------------------------------------------------------*/
bool
IsWiFiConnected()
{
	return g_wifiConnected;
}
/*-------------------------------------------------------------------*/
void
SetMobileToUse() {
	struct in_addr dprox_addr;
	int val = inet_aton(DPROX_IP, &dprox_addr);
	//	dtp_setiface((uint32_t)dprox_addr.s_addr, "rmnet0", sizeof("rmnet0"));
	g_wifiActivated = false;
}
/*-------------------------------------------------------------------*/
void
SetWiFiToUse() {
	struct in_addr dprox_addr;
	int val = inet_aton(DPROX_IP, &dprox_addr);
	//	dtp_setiface((uint32_t)dprox_addr.s_addr, "wlan0", sizeof("wlan0"));
	g_wifiActivated = true;
}

/*-------------------------------------------------------------------*/
int
GetCurrentNetworkType() {

	if (IsWiFiConnected())
		return 1;
	else if (IsMobileConnected())
		return 2;
	
	return 0;
}
/*-------------------------------------------------------------------*/
bool
IsAvailConnection(dtp_context *ctx)
{
	bool ret;
	if (g_useScheduler) {
		if (ctx->tc_state == SOCK_ESTABLISHED)
			return ((!ctx->tc_isDeadlineSet) || ctx->tc_scheduleSend);
		else
			return true;
	}
	else {
		ret = (((g_mobileConnected && ctx->tc_useMobile) || g_wifiConnected)
				&& ((ctx->tc_state != SOCK_ESTABLISHED ||
					ctx->tc_rcvLoBufHB == 0) ? 1 :
					(ctx->tc_limitRcvBuf ?
					 ctx->tc_rcvLoBufHB > GetReadBufDataSize(ctx) : 1)));
		if (ret == false)
			DHK_TFLOG(DHK_DEBUG & DRCVLOBUF & 0, DHK_F_BASE"avail.txt",
					"IsAvailConnection return false\n"
					"(((%d && %d) || %d)\n"
					"&& ((%d != %d ||\n"
					"%d == 0) ? 1 :\n"
					"(%d ?\n"
					"%d > %d : 1)))\n",
					g_mobileConnected, ctx->tc_useMobile, g_wifiConnected,
					ctx->tc_state, SOCK_ESTABLISHED,
					ctx->tc_rcvLoBufHB,
					ctx->tc_limitRcvBuf,
					ctx->tc_rcvLoBufHB, GetReadBufDataSize(ctx));

		return ret;
	}
}
/*-------------------------------------------------------------------*/
static void
DTPStartMobileBWMeasuring (dtp_context *ctx)
{
	ctx->tc_isMobileBWMeasuring = TRUE;
	ctx->tc_mobileCounter = 0;
	ctx->tc_lastBlockRemain = ctx->tc_blockRemain;
}
/*-------------------------------------------------------------------*/
static void
DTPStopMobileBWMeasuring (dtp_context *ctx)
{
	ctx->tc_isMobileBWMeasuring = FALSE;
}
/*-------------------------------------------------------------------*/
static void
DTPCalculateMobileBW (dtp_context *ctx)
{
	ctx->tc_mobileSpeed = ((double)(ctx->tc_lastBlockRemain - ctx->tc_blockRemain)) / 5.0;

	ctx->tc_mobileCounter = 0;
	ctx->tc_lastBlockRemain = ctx->tc_blockRemain;
}
/*-------------------------------------------------------------------*/
static void 
HandleDeadline(void)
{	
    dtp_context *ctx;
    struct timeval tv;
    double diff, now;

    if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, );
    }
    now = tv.tv_sec + (tv.tv_usec / 1e6);

    if (pthread_mutex_lock(GetRetransQueueLock())) {
		TRACE_ERR("pthread_mutex_lock() failed");
		EXIT(-1, return);
    }
	
#ifdef HAVE_SCHEDULER
	LOGD("SendSchedulerMessage() is called");
	SendSchedulerMessage();
#endif
	
	if ((ctx = TAILQ_FIRST(GetRetransQueue())) != NULL) {
		if (ctx->tc_scheduleSend) {
			double total = 0;
			TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
				total += (double)(ctx->tc_lastBlockRemain - ctx->tc_blockRemain);
				ctx->tc_lastBlockRemain = ctx->tc_blockRemain;
			}				
			if (g_wifiConnected && total > 0) {
				if (g_wifiActivated)
					g_wifiSpeed = 8.0 * total / 1000.0 / 1000.0 / 5.0;
				else
					g_mobileSpeed = 8.0 * total / 1000.0 / 1000.0 / 5.0;
			}
			else if (g_mobileConnected && total > 0)
				g_mobileSpeed = 8.0 * total / 1000.0 / 1000.0 / 5.0;
		}
	}
	
    /* context queue */
    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
			
		if (ctx->tc_isDeadlineSet && ctx->tc_deadline > 0) {

			diff = ctx->tc_deadlineTime.tv_sec + 
				(ctx->tc_deadlineTime.tv_usec / 1e6) - now;

			/* if deadline expires, close the connection */
			if (diff <=  0) {//- DEADLINE_LAST_TIMEOUT) {
				DTPCloseConnection(ctx);
				continue;
			}

			/* if there is no scheduler available */
			if (!g_useScheduler) {
				diff = ctx->tc_deadlineTime.tv_sec + 
					(ctx->tc_deadlineTime.tv_usec / 1e6) - now;
				
				/* set next deadline after sending a block */
				/*
				  if (ctx->tc_blockRemain <= 0) {
				  ctx->tc_blockRemain = ctx->tc_blockSize;
				  ctx->tc_deadlineTime.tv_sec = tv.tv_sec + ctx->tc_deadline;
				  ctx->tc_deadlineTime.tv_usec = tv.tv_usec;
				  } 
				*/
				
				if (ctx->tc_blockRemain >= ((diff - 30) * ctx->tc_mobileSpeed)
					&& ctx->tc_isWifiOnly == FALSE) {
					if (ctx->tc_useMobile == FALSE) {
						ctx->tc_useMobile = TRUE;
					}
				}
				else if (ctx->tc_deadline > 0 && !g_wifiConnected) {
					if (ctx->tc_useMobile == TRUE) {
						ctx->tc_useMobile = FALSE;
					}
				}

				LOGD("[sock %d] blockRemain = %d / diff = %.1f / mSpeed = %d / tc_useMobile = %d / mob = %d / wifi = %d",
					 ctx->tc_sock, ctx->tc_blockRemain, diff, (int)ctx->tc_mobileSpeed, ctx->tc_useMobile, g_mobileConnected, g_wifiConnected); 

				/* mobile BW measurement */
				if ((ctx->tc_useMobile && g_mobileConnected) &&
					!(ctx->tc_isMobileBWMeasuring)) {
					DTPStartMobileBWMeasuring(ctx);
				}

				if (!(ctx->tc_useMobile && g_mobileConnected) &&
					ctx->tc_isMobileBWMeasuring) {
					DTPStopMobileBWMeasuring(ctx);
				}
				
				if (ctx->tc_isMobileBWMeasuring) {
					if (++(ctx->tc_mobileCounter) == 5) {
						DTPCalculateMobileBW(ctx);
					}
				}

			}	
		}
	}

	if (g_useScheduler) {
		Schedule(g_wifiSpeed, g_mobileSpeed);
	}

    TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
		if (ctx->tc_state == SOCK_ESTABLISHED/* && IsAvailConnection(ctx)*/) {
			ctx->tc_schedAllowTime += 1;
			DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
			DTPSendACKPacket(ctx);
			LOGD("[sock %d] DTP Send ACK", ctx->tc_sock);
		}
	}

    if (pthread_mutex_unlock(GetRetransQueueLock())) {
		TRACE_ERR("pthread_mutex_unlock() failed");
		EXIT(-1, );
    }

}

/*-------------------------------------------------------------------*/
static void
DTPDeadlineEvent(int sock, short event, void *arg)
{
    /* timer timeout */
    if (event & EV_TIMEOUT) {
		/* timeout for retransmission */
		HandleDeadline();
	}
}

/*-------------------------------------------------------------------*/
void
DTPHandleWifiEvent(int fd, short events, void *arg)
{
	dtp_context* ctx;
    char buffer[4096];
    struct nlmsghdr *nlh;
    int len;
	int wlan_event = 0;
	int rmnet_event = 0;
	double diff, now;
    struct timeval tv;
	uint32_t ipaddrNow = 0;
	
	if (gettimeofday(&tv, NULL)) {
		perror("gettimeofday() failed");
		EXIT(-1, );
    }
    now = tv.tv_sec + (tv.tv_usec / 1e6);

	int indextoname_err = 0;

	nlh = (struct nlmsghdr *)buffer;
    
	while ((len = recv(fd, nlh, 4096, 0)) > 0) {

		while ((NLMSG_OK(nlh, len)) && (nlh->nlmsg_type != NLMSG_DONE)) {

			if (nlh->nlmsg_type == RTM_NEWADDR) {
				struct ifaddrmsg *ifa = (struct ifaddrmsg *) NLMSG_DATA(nlh);
				struct rtattr *rth = IFA_RTA(ifa);
				int rtl = IFA_PAYLOAD(nlh);

				while (rtl && RTA_OK(rth, rtl)) {
					if (rth->rta_type == IFA_LOCAL) {
						char name[IFNAMSIZ];
						if_indextoname(ifa->ifa_index, name);
						if (!strncmp(name, "wlan", 4)) {
							DHK_TFLOG(DHK_DEBUG & DRCVLOBUF,
									DHK_F_BASE"rcvlobuf.txt",
									"wlan connected");
							wlan_event = RTM_NEWADDR;
						}
						else if (!strncmp(name, "rmnet", 5)) {
							DHK_TFLOG(DHK_DEBUG & DRCVLOBUF,
									DHK_F_BASE"rcvlobuf.txt",
									"cellular connected");
							rmnet_event = RTM_NEWADDR;
						}
						else
							indextoname_err = 1;

						uint32_t ipaddr = htonl(*((uint32_t *)RTA_DATA(rth)));
						LOGD("[RTM_NEWADDR] %s is now %d.%d.%d.%d\n",
							 name,
							 (ipaddr >> 24) & 0xff,
							 (ipaddr >> 16) & 0xff,
							 (ipaddr >> 8) & 0xff,
							 ipaddr & 0xff);

						ipaddrNow = ipaddr;
					}
					rth = RTA_NEXT(rth, rtl);
				}
			}

			if (nlh->nlmsg_type == RTM_DELADDR) {
				struct ifaddrmsg *ifa = (struct ifaddrmsg *) NLMSG_DATA(nlh);
				struct rtattr *rth = IFA_RTA(ifa);
				int rtl = IFA_PAYLOAD(nlh);

				while (rtl && RTA_OK(rth, rtl)) {
					if (rth->rta_type == IFA_LOCAL) {
						char name[IFNAMSIZ];
						char* ret = if_indextoname(ifa->ifa_index, name);
						if (ret == NULL)
							fprintf(stderr, "indextoname ret = NULL, errno = %d", errno);
						if (!strncmp(name, "wlan", 4)) {
							DHK_TFLOG(DHK_DEBUG & DRCVLOBUF,
									DHK_F_BASE"rcvlobuf.txt",
									"wlan disconnected");
							wlan_event = RTM_DELADDR;
						}
						else if (!strncmp(name, "rmnet", 5)) {
							DHK_TFLOG(DHK_DEBUG & DRCVLOBUF,
									DHK_F_BASE"rcvlobuf.txt",
									"cellular disconnected");
							rmnet_event = RTM_DELADDR;
						}
						else
							indextoname_err = 1;

						// LOG FOR DEBUGGING
						uint32_t ipaddr = htonl(*((uint32_t *)RTA_DATA(rth)));						   
						LOGD("[RTM_DELADDR] %s is removed from %d.%d.%d.%d\n",
							 name,
							 (ipaddr >> 24) & 0xff,
							 (ipaddr >> 16) & 0xff,
							 (ipaddr >> 8) & 0xff,
							 ipaddr & 0xff);
						
					}
					rth = RTA_NEXT(rth, rtl);
				}
			}

			nlh = NLMSG_NEXT(nlh, len);
		}
	}

	if (wlan_event == RTM_NEWADDR) {
		g_wifiConnected = true;
	}
	if (wlan_event == RTM_DELADDR) {
		g_wifiConnected = false;
	}
	if (rmnet_event == RTM_NEWADDR) {
		g_mobileConnected = true;
	}
	if (rmnet_event == RTM_DELADDR) {
		g_mobileConnected = false;
	}
	
	if (indextoname_err == 1) {
		// LOG FOR DEBUGGING
		char ifname[100];
		dtp_getifacename(0, ifname, 100);
		if (!strncmp(ifname, "wlan", 4)) {
			g_wifiConnected = true;
			g_mobileConnected = false;
		}
		else if (!strncmp(ifname, "rmnet", 5)) {
			g_wifiConnected = false;			
			g_mobileConnected = true;				
		}
		else {
			g_wifiConnected = false;			
			g_mobileConnected = false;				
		}
		LOGD("wifi = %d / mobile = %d (fixed)", g_wifiConnected, g_mobileConnected);
	}

	TAILQ_FOREACH(ctx, GetRetransQueue(), tc_link) {
		if (wlan_event == RTM_NEWADDR)
			ctx->tc_wifiAvailable = 1;
		if (wlan_event == RTM_DELADDR)
			ctx->tc_wifiAvailable = 0;
		if (rmnet_event == RTM_NEWADDR)
			ctx->tc_mobileAvailable = 1;
		if (rmnet_event == RTM_DELADDR)
			ctx->tc_mobileAvailable = 0;

		if (g_wifiConnected == false && g_mobileConnected == false) {
			
			/*
			  if (ctx->tc_isNetConnected) {
			  LOGD("DISCONNECTED : calling HandleDisconnect\n");
			  HandleDisconnect(ctx);
			  }
			*/ //YGMOON
			
		}
		else {			
			//		LOGD("[new interface] send ACKs in retTimerQHead\n");
			// send ACK packet with changed address
			if (ctx->tc_state == SOCK_ESTABLISHED) {
				DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
				DTPSendACKPacket(ctx);
				ctx->tc_isNetConnected = TRUE;
			}

			diff = ctx->tc_deadlineTime.tv_sec + 
				(ctx->tc_deadlineTime.tv_usec / 1e6) - now;
			
			/* set next deadline after sending a block */
			/*
			  if (ctx->tc_blockRemain <= 0) {
			  ctx->tc_blockRemain = ctx->tc_blockSize;
			  ctx->tc_deadlineTime.tv_sec = tv.tv_sec + ctx->tc_deadline;
			  ctx->tc_deadlineTime.tv_usec = tv.tv_usec;
			  } 
			*/

			LOGD("[sock %d] blockRemain = %d / diff = %.1f / mSpeed = %d",
				 ctx->tc_sock, ctx->tc_blockRemain, diff, (int)ctx->tc_mobileSpeed); 
			
			if (ctx->tc_blockRemain >= (diff * ctx->tc_mobileSpeed)
				&& ctx->tc_isWifiOnly == FALSE) {
				if (ctx->tc_useMobile == FALSE) {
					ctx->tc_useMobile = TRUE;
				}
			}
			else if (ctx->tc_deadline > 0 && !g_wifiConnected) {
				if (ctx->tc_useMobile == TRUE) {
					ctx->tc_useMobile = FALSE;
				}
			}
				
			/* mobile BW measurement */
			if ((ctx->tc_useMobile && g_mobileConnected) &&
				!(ctx->tc_isMobileBWMeasuring)) {
				DTPStartMobileBWMeasuring(ctx);
			}
				
			if (!(ctx->tc_useMobile && g_mobileConnected) &&
				ctx->tc_isMobileBWMeasuring) {
				DTPStopMobileBWMeasuring(ctx);
			}
				
			if (ctx->tc_isMobileBWMeasuring) {
				if (++(ctx->tc_mobileCounter) == 5) {
					DTPCalculateMobileBW(ctx);
				}
			}
		} // end of if-else statement
	   
		LOGD("wifi = %d / mobile = %d", g_wifiConnected, g_mobileConnected);

		struct timeval tvv;
		gettimeofday(&tvv, NULL);
		/*
		if (g_wifiConnected == 1 && g_mobileConnected == 1) {
			struct in_addr dprox_addr;
			int val = inet_aton(DPROX_IP, &dprox_addr);
			dtp_setiface((uint32_t)dprox_addr.s_addr, "wlan0", sizeof("wlan0"));

			if (ctx->tc_state == SOCK_ESTABLISHED) {
				DHK_FDBG(DHK_DEBUG & DACK, DHK_F_BASE"ack.txt", " ");
				DTPSendACKPacket(ctx);
				ctx->tc_isNetConnected = TRUE;
			}
		}
		*/

		/* initializing interface status */
		char ifname[100];
		memset(ifname, 0, 100);
		dtp_getifacename(0, ifname, 100);	
		
		if (networkTypeLast != GetCurrentNetworkType() && ctx->tc_state == SOCK_ESTABLISHED) {			/*
			char filename[500];
			char today[50];
			strftime(today, 50, "%Y%m%d", localtime(&tv.tv_sec));
			sprintf(filename, "%s%s_D_%s.txt", GetSDCardPath(), today, GetGeneratedHostID());
			
			FILE* fd2;
			fd2 = fopen(filename, "a+");
			if (fd2 != NULL) {
				fprintf(fd2, "%u %d %d.%d.%d.%d %s %d %.3f %.3f %d\n",
						ctx->tc_flowID,
						networkTypeLast,
						(ipaddrLast >> 24)&0xff,(ipaddrLast >> 16)&0xff,
						(ipaddrLast >> 8)&0xff, ipaddrLast&0xff,
						//	inet_ntoa(ctx->tc_sockAddr.sin_addr),
						inet_ntoa(ctx->tc_peerAddr.sin_addr),
						ctx->tc_blockRemainLast - ctx->tc_blockRemain,
						(ctx->tc_networkTimeLast)? ctx->tc_networkTimeLast : now,
						(ctx->tc_networkTimeLast)? now - ctx->tc_networkTimeLast : 0,
						ctx->tc_schedAllowTime);
				fflush(fd2);
				fclose(fd2);	
			}

			LOGP("[%u %d %d.%d.%d.%d %s %d %.3f %.3f %d]",
				 ctx->tc_flowID, 
				 networkTypeLast, 
				 (ipaddrLast >> 24)&0xff,(ipaddrLast >> 16)&0xff,
				 (ipaddrLast >> 8)&0xff, ipaddrLast&0xff,
				 //				 inet_ntoa(ctx->tc_sockAddr.sin_addr),
				 inet_ntoa(ctx->tc_peerAddr.sin_addr),
				 ctx->tc_blockRemainLast - ctx->tc_blockRemain,
				 (ctx->tc_networkTimeLast)? ctx->tc_networkTimeLast : now,
				 (ctx->tc_networkTimeLast)? now - ctx->tc_networkTimeLast : 0,
				 ctx->tc_schedAllowTime);
        */			
			ipaddrLast = ipaddrNow; // update ip addr
			ctx->tc_networkTimeLast = now;
			ctx->tc_blockRemainLast = ctx->tc_blockRemain;
			ctx->tc_schedAllowTime = 0;
		}
	}
	networkTypeLast = GetCurrentNetworkType();

	// DTN : TO SCHEDULE USING THE NEW NETWORK STATUS
}

/*-------------------------------------------------------------------*/
void
DTPMobileContextInit(dtp_context* new_ctx) {
	new_ctx->tc_useMobile = TRUE;

	/* mobile network speed */
	new_ctx->tc_mobileSpeed = INIT_MOBILE_SPEED;

	new_ctx->tc_wifiAvailable = g_wifiConnected;
	new_ctx->tc_mobileAvailable = g_mobileConnected;

	new_ctx->tc_wifiAvailable = 0;
	new_ctx->tc_mobileAvailable = 1;
	new_ctx->tc_limitRcvBuf == 1;

#if 0
	char name[20] = {0};
	if (dtp_getifacename(DPROX_IP_HEX, name, 20))
		new_ctx->tc_limitRcvBuf = 1;
	else if (!strncmp(name, "rmnet", 5))
		new_ctx->tc_limitRcvBuf = 1;
#endif
#if 0
	struct ifaddrs *ifaddr;
	if (getifaddrs(&ifaddr)) {
		new_ctx->tc_wifiAvailable = g_wifiConnected;
		new_ctx->tc_mobileAvailable = g_mobileConnected;
		DHK_TFLOG(DHK_DEBUG & DRCVLOBUF, DHK_F_BASE"rcvlobuf.txt",
				"getifaddr fail: %s", strerror(errno));
	}
	else {
		while (ifaddr != NULL) {
			if (!strncmp(ifaddr->ifa_name, "wlan", 4)
					&& ifaddr->ifa_addr != NULL)
				new_ctx->tc_wifiAvailable = 1;
			else if (!strncmp(ifaddr->ifa_name, "rmnet", 5)
					&& ifaddr->ifa_addr != NULL)
				new_ctx->tc_mobileAvailable = 1;

			ifaddr = ifaddr->ifa_next;
		}

		DHK_TFLOG(DHK_DEBUG & DRCVLOBUF, DHK_F_BASE"rcvlobuf.txt",
				"wlan: %s, rmnet: %s",
				new_ctx->tc_wifiAvailable ? "available" : "none",
				new_ctx->tc_mobileAvailable ? "available" : "none");
	}
#endif // IN_MOBILE
}

/*-------------------------------------------------------------------*/
void
DTPMobileEventInit(struct event_base* base)
{
	LOGP("DTPMobileEventInit");
	int res;
    int sock;
    struct event *wifi_event;
    struct event *deadline_event;
	struct sockaddr_nl addr;
    struct timeval tv;

#ifdef HAVE_SCHEDULER
    tv.tv_sec = SCHED_TIMEOUT;    /* timeout timer (20 second) */
    tv.tv_usec = 0;
#else
    tv.tv_sec = 5;
    tv.tv_usec = 0;
#endif
	g_useScheduler
 = 1;

    deadline_event = event_new(base, -1, EV_PERSIST, DTPDeadlineEvent, NULL);
    if (!deadline_event) {
		TRACE("event_new failed\n");
		EXIT(-1, return);
    }

    res = event_add(deadline_event, &tv);
    if (res) {
		TRACE("event_add failed\n");
		EXIT(-1, return);
    }

	/* initializing interface status */
	char ifname[100];
	dtp_getifacename(0, ifname, 100);
	
	if (!strncmp(ifname, "wlan", 4)) {
		g_wifiConnected = true;
		g_mobileConnected = false;
	}
	else if (!strncmp(ifname, "rmnet", 5)) {
		g_wifiConnected = false;			
		g_mobileConnected = true;				
	}
	networkTypeLast = GetCurrentNetworkType();
	LOGD("wifi = %d / mobile = %d (init)", g_wifiConnected, g_mobileConnected);

	int fd;
	struct ifreq ifr;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	ifr.ifr_addr.sa_family = AF_INET;
	strncpy(ifr.ifr_name, ifname, IFNAMSIZ-1);
	ioctl(fd, SIOCGIFADDR, &ifr);
	close(fd);
	LOGD("==%s==", inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr));
	ipaddrLast = ntohl((((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr).s_addr);
	LOGD("==%u==", ipaddrLast);

    if ((sock = socket(PF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
        LOGD("couldn't open NETLINK_ROUTE socket");
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) != 0) {
		LOGD("socket() error: fcntl error");
		EXIT(-1, );
    }	

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_groups = RTMGRP_IPV4_IFADDR;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        LOGD("couldn't bind");
    }

    wifi_event = event_new(base, sock, EV_READ|EV_PERSIST,
						   DTPHandleWifiEvent, (void*)base);
    ASSERT(wifi_event, return);
    res = event_add(wifi_event, NULL);
    if (res) {
		TRACE("event_add failed\n");
		EXIT(-1, return);
    }
}
/*-------------------------------------------------------------------*/
#endif /* IN_MOBILE */

