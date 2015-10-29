#ifndef _DEBUG_H_
#define _DEBUG_H_
#include <errno.h>
#include <stdio.h>
#include <string.h>

//#define DEBUGX
#ifdef DEBUGX
#define TRACE(fmt, msg...) {						\
	fprintf(stderr, "[%s] (%s:%d) " fmt,				\
		__FUNCTION__, __FILE__, __LINE__, ##msg);		\
    }         

#else
#define TRACE(fmt, msg...) (void)0
#endif

#define TRACE_ERR(fmt, msg...) {					\
	fprintf(stderr, "[%s] (%s:%d) " fmt ":errno=%d errstr=%s",	\
		__FUNCTION__, __FILE__, __LINE__,			\
		##msg, errno, strerror(errno));				\
    }  

#define TRACEX(fmt, msg...) {						\
	fprintf(stderr, "[%s] (%s:%d) " fmt,				\
		__FUNCTION__, __FILE__, __LINE__, ##msg);		\
    }

#ifdef IN_MOBILE
#include <android/log.h>

#define LOGV(...)   __android_log_print(ANDROID_LOG_VERBOSE, "libdtp", __VA_ARGS__)
#define LOGD(...)   __android_log_print(ANDROID_LOG_DEBUG, "libdtp", __VA_ARGS__)
#define LOGI(...)   __android_log_print(ANDROID_LOG_INFO, "libdtp", __VA_ARGS__)
#define LOGW(...)   __android_log_print(ANDROID_LOG_WARN, "libdtp", __VA_ARGS__)
#define LOGE(...)   __android_log_print(ANDROID_LOG_ERROR, "libdtp", __VA_ARGS__)
#define LOGP(...)   __android_log_print(ANDROID_LOG_VERBOSE, "dtn_log", __VA_ARGS__)
#define LOGC(...)   __android_log_print(ANDROID_LOG_VERBOSE, "cedos", __VA_ARGS__)

#else

   #define LOGV(...) (void)0
   #define LOGD(...) (void)0
   #define LOGI(...) (void)0
   #define LOGW(...) (void)0
   #define LOGE(...) (void)0
   #define LOGP(...) (void)0
   #define LOGC(...) (void)0

#endif

#endif /* _DEBUG_H_ */

extern void DTPPrintPacket(const u_char* p, int totalLen);
extern void DTPPrintTimestamp(char* c);
extern void DTPTimerStart();
extern void DTPTimerEnd(char *c);
