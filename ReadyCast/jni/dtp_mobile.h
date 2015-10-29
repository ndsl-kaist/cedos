#ifndef _DTP_MOBILE_H_
#define _DTP_MOBILE_H_

#ifdef IN_MOBILE
#include "scheduler.h"
static bool g_mobileConnected = false;   /* mobile network connected */
static bool g_wifiConnected = false;     /* wifi network connected */
static bool g_wifiActivated = true;     /* wifi network used when connected */

static double g_mobileSpeed = 1.0; /* Mbps */
static double g_wifiSpeed = 1.0; /* Mbps */

#ifdef HAVE_SCHEDULER
static bool g_useScheduler = true;
#else
static bool g_useScheduler = false;
#endif
#else
#include <sched.h>
#endif

extern void DTPMobileEventInit(struct event_base* base);
extern void DTPMobileContextInit(dtp_context* new_ctx);

extern void DTPHandleWifiEvent(int fd, short events, void *arg);

extern bool IsMobileConnected();
extern bool IsWiFiConnected();
extern bool IsAvailConnection(dtp_context *ctx);
//extern void HandleDeadline(void);
//extern void DTPStartMobileBWMeasuring (dtp_context *ctx);
//extern void DTPStopMobileBWMeasuring (dtp_context *ctx);
//extern void DTPCalculateMobileBW (dtp_context *ctx);

extern bool IsWiFiConnected();
extern bool IsAvailConnection(dtp_context *ctx);

extern int GetNetworkTypeLast();
extern uint32_t GetIpaddrLast();

#endif
