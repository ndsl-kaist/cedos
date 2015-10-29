
#ifndef __DHKIM_DEBUG_H_
#define __DHKIM_DEBUG_H_

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <pthread.h>

/* set level, info */
#ifdef DTP
#error DTP is already defined!
#endif

#if 1

#define DHK_F_BASE	"/sdcard""/"

#define DDTP				(1<<0)	//DTP. dtran.txt, ddbg.txt
#define DPKT				(1<<1)	//DTP packet in/out
#define DPKTHDR				(1<<2)	//Show DTP packet header
#define DTRANS				(1<<3)	//dtp transport layer
#define DEVENT				(1<<4)	//Debug dtp event loop
#define DDUMP				(1<<5)	//Dump DTP payload
#define DMASTER				(1<<6)	//Master thread
#define DWORKER				(1<<7)	//Worker thread
#define DUTIL				(1<<8)	//util.c
#define DERROR				(1<<9)	//Log error when exception occur
#define DTEMP				(1<<10)	//create temp.txt which have debug
									//info for increasing FDs
#define DDEADLOCK01			(1<<11)	//ProcessAggregatePacket() ... DTPSendEventTOLibThread()
#define DWARNING			(1<<12)	//Log warnings
#define DRETRANSMIT			(1<<13)	//Log retransmitted packets
#define DFINPKT				(1<<14)	//Log FIN packets
#define DFLOW				(1<<15)	//Log flow lifecycle
#define DPAYLOAD			(1<<16)	//Dump payload diff
#define DMISMATCH			(1<<17) //Debug mismatch
#define DCHG				(1<<18) //Debug <SYN, CHG, CHG, ...>
#define DACK				(1<<19) //Debug <ACK, ACK, SYN, ...>
#define DRCVLOBUF			(1<<20) //Debug min-req feature
#define DOLD				(1<<21) //Debug min-req feature
#define DTRACK				(1<<22) //Debug min-req feature

//#define DHK_LOCK
#define DHK_DEBUG 		(0	\
	| DERROR				\
	| DRCVLOBUF				\
	| DPKT					\
		)
/*
	| DDTP					\
	| DPKT					\
	| DPKTHDR				\
	| DDUMP					\
	| DDEADLOCK01			\
	| DRETRANSMIT			\
	| DTRANS				\
	| DTEMP					\
	| DFINPKT				\
	| DWARNING				\
	| DMISMATCH				\

	| DMASTER				\
	| DWORKER				\
	| DUTIL					\
*/
#endif

/*  -- DEBUG --  */
#ifdef DHK_DEBUG

#define MAX_PATH_LEN	20

#define CUTOFF_STR(string, max_len)								\
	(char*)(string + (size_t)((strlen(string) - max_len) > 0 ?	\
				(strlen(string) - max_len) : 0))


#define DHK_PRINT(var, format)				\
do {										\
	printf("%s: "format"\n", #var, var);	\
} while (0)

#define DHK_MEM(condition, file_name, buffer, len)		\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		FILE *dhk_fd;									\
		int dhk_i, dhk_j, dhk_k, *data;								\
		char *dhk_bytes;									\
		dhk_fd = fopen(file_name, "a+");				\
		if (dhk_fd != NULL) {							\
		fprintf(dhk_fd, "%s:%d:%s: === MEM DUMP ===\n",	\
				CUTOFF_STR(__FILE__, MAX_PATH_LEN), __LINE__, __func__);			\
		fprintf(dhk_fd, "M      L       -BIN-                      -HEX-      -CHR-\n");	\
		data = (int*)(buffer);							\
		for (dhk_i = 0; dhk_i <= (len-1)/4; dhk_i++) {				\
			dhk_bytes = (char*)&(data[dhk_i]);					\
			for (dhk_j = 0; dhk_j < 4; dhk_j++) {					\
				if (dhk_i*4+dhk_j < (len))						\
					for (dhk_k = 7; dhk_k >= 0; dhk_k--)				\
						fprintf(dhk_fd, "%d", (dhk_bytes[dhk_j]&(1<<dhk_k))>>dhk_k);	\
				else									\
					fprintf(dhk_fd, "        ");		\
				fprintf(dhk_fd, " ");					\
			}											\
			fprintf(dhk_fd, " | ");						\
			for (dhk_j = 0; dhk_j < 4; dhk_j++) {					\
				if (dhk_i*4+dhk_j < (len))						\
					fprintf(dhk_fd, "%02X", dhk_bytes[dhk_j] & 0xFF);	\
				else									\
					fprintf(dhk_fd, "  ");				\
				fprintf(dhk_fd, " ");					\
			}											\
			fprintf(dhk_fd, " | ");						\
			for (dhk_j = 0; dhk_j < 4; dhk_j++) {					\
				if (dhk_i*4+dhk_j < (len)) {					\
					if (32<=dhk_bytes[dhk_j] && dhk_bytes[dhk_j]<=126)	\
						fprintf(dhk_fd, "%c", dhk_bytes[dhk_j] & 0xFF);	\
					else								\
						fprintf(dhk_fd, ".");			\
				}										\
				else									\
					fprintf(dhk_fd, " ");				\
			}											\
														\
			fprintf(dhk_fd, "\n");						\
		}												\
		fflush(dhk_fd);									\
		fflush(dhk_fd);									\
		fclose(dhk_fd);									\
		errno = errno_bkup;								\
		}												\
	}} while (0)

#define DHK_TFLOG(condition, file_name, args...)		\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		FILE *dhk_fd;									\
		struct timeval dhk_curTime;						\
		int dhk_milli;									\
		char dhk_timeString[30];						\
		dhk_fd = fopen(file_name, "a+");				\
		if (dhk_fd != NULL) {							\
		gettimeofday(&dhk_curTime, NULL);				\
		dhk_milli = dhk_curTime.tv_usec / 1000;			\
		strftime(dhk_timeString, 30, "%m-%d-%H-%M-%S", 	\
				localtime(&dhk_curTime.tv_sec));		\
		fprintf(dhk_fd, "%s-%03d: ", 					\
				dhk_timeString, dhk_milli);				\
		fprintf(dhk_fd, args); 							\
		fprintf(dhk_fd, "\n");							\
		fflush(dhk_fd);									\
		fflush(dhk_fd);									\
		fclose(dhk_fd);									\
		errno = errno_bkup;								\
		}												\
	}} while (0)

#define DHK_TFDBG(condition, file_name, args...)		\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		FILE *dhk_fd;									\
		struct timeval dhk_curTime;						\
		int dhk_milli;									\
		char dhk_timeString[30];						\
		dhk_fd = fopen(file_name, "a+");				\
		if (dhk_fd != NULL) {							\
		gettimeofday(&dhk_curTime, NULL);				\
		dhk_milli = dhk_curTime.tv_usec / 1000;			\
		strftime(dhk_timeString, 30, "%m-%d-%H-%M-%S", 	\
				localtime(&dhk_curTime.tv_sec));		\
		fprintf(dhk_fd, "%s-%03d:%s:%d:%s: ", 			\
				dhk_timeString, dhk_milli,				\
				CUTOFF_STR(__FILE__, MAX_PATH_LEN), __LINE__, __func__);\
		fprintf(dhk_fd, args); 							\
		fprintf(dhk_fd, "\n");							\
		fclose(dhk_fd);									\
		errno = errno_bkup;								\
		}												\
	}} while (0)

#define DHK_DUMP(condition, file_name, buffer, len)		\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		int dhk_fd;										\
		dhk_fd = open(file_name, O_WRONLY | O_APPEND | O_CREAT);	\
		if (dhk_fd > 0) {								\
			write(dhk_fd, (const void*)buffer, (size_t)len);	\
			close(dhk_fd);								\
		}												\
		errno = errno_bkup;								\
	}} while (0)

#define DHK_FLOG(condition, file_name, args...)			\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		FILE *dhk_fd;									\
		dhk_fd = fopen(file_name, "a+");				\
		if (dhk_fd != NULL) {							\
			fprintf(dhk_fd, args); 						\
			fprintf(dhk_fd, "\n");						\
			fflush(dhk_fd);								\
			fclose(dhk_fd);								\
		}												\
		errno = errno_bkup;								\
	}} while (0)

#define DHK_FDBG(condition, file_name, args...)			\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		FILE *dhk_fd;									\
		dhk_fd = fopen(file_name, "a+");				\
		if (dhk_fd != NULL) {							\
			fprintf(dhk_fd, "%s:%d:%s: ", 				\
					CUTOFF_STR(__FILE__, MAX_PATH_LEN), __LINE__, __func__);		\
			fprintf(dhk_fd, args); 						\
			fprintf(dhk_fd, "\n");						\
			fflush(dhk_fd);								\
			fclose(dhk_fd);								\
		}												\
		errno = errno_bkup;								\
	}} while (0)

#define DHK_DBG(condition, args...)						\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		printf("%s:%d:%s: ",							\
				CUTOFF_STR(__FILE__, MAX_PATH_LEN), __LINE__, __func__);		\
		printf(args); 									\
		printf("\n"); 									\
		errno = errno_bkup;								\
	}} while (0)

#define DHK_DBG_STEP(condition, args...)				\
do{ if (condition) {									\
		int errno_bkup = errno;							\
		 DBG(1, args); 									\
		 getchar(); 									\
		errno = errno_bkup;								\
	}} while (0)

#define ASSERT(cond, action...)										\
{																	\
	if (!(cond)) {													\
		DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",			\
				"do '%s' instead of 'assert(%s);'", #action, #cond);\
		action;														\
	}																\
}

#define EXIT(value, action...)											\
{																	\
	DHK_FDBG(DHK_DEBUG & DERROR, DHK_F_BASE"error.txt",				\
			"do '%s' instead of 'exit(%s);'", #action, #value);		\
	action;															\
}

#else // DHK_DEBUG

#define ASSERT(cond, args...) assert(cond)
#define EXIT(value, args...) exit(value)

#define DHK_PRINT(args...)
#define DHK_MEM(args...)
#define DHK_TFLOG(args...)
#define DHK_TFDBG(args...)
#define DHK_DUMP(args...)
#define DHK_FLOG(args...)
#define DHK_FDBG(args...)
#define DHK_DBG(args...)
#define DHK_DBG_STEP(args...)

#endif // DHK_DEBUG

#ifdef DHK_LOCK

#define LOCK_LOG_COND	0
//#define LOCK_LOG_COND	strcmp(func, "HandleTimeout")

static int dhk_mutex_lock(const char *func, int line, pthread_mutex_t *mutex,
		char *n_mutex)
{
	int err;
	DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
			"%08X:%s:%d TRY mutex_lock(%s)",
			(unsigned int)pthread_self(), func, line, n_mutex);
	err = pthread_mutex_lock(mutex);
	if (err)
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d FAILED mutex_lock(%s), %d:%s",
				(unsigned int)pthread_self(), func, line, n_mutex, err, strerror(err));
	else
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d PASSED mutex_lock(%s)",
				(unsigned int)pthread_self(), func, line, n_mutex);
	return err;
}
static int dhk_mutex_trylock(const char *func, int line, pthread_mutex_t *mutex,
		char *n_mutex)
{
	int err;
	DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
			"%08X:%s:%d TRY mutex_trylock(%s)",
			(unsigned int)pthread_self(), func, line, n_mutex);
	err = pthread_mutex_trylock(mutex);
	if (err)
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d FAILED mutex_trylock(%s), %d:%s",
				(unsigned int)pthread_self(), func, line, n_mutex, err, strerror(err));
#if 0
	else
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d PASSED mutex_trylock(%s)",
				(unsigned int)pthread_self(), func, line, n_mutex);
#endif
	return err;
}
static int dhk_mutex_unlock(const char *func, int line, pthread_mutex_t *mutex,
		char *n_mutex)
{
	int err;
	DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
			"%08X:%s:%d TRY mutex_unlock(%s)",
			(unsigned int)pthread_self(), func, line, n_mutex);
	err = pthread_mutex_unlock(mutex);
	if (err)
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d FAILED mutex_unlock(%s), %d:%s",
				(unsigned int)pthread_self(), func, line, n_mutex, err, strerror(err));
#if 0
	else
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d PASSED mutex_unlock(%s)",
				(unsigned int)pthread_self(), func, line, n_mutex);
#endif
	return err;
}
static int dhk_cond_broadcast(const char *func, int line, pthread_cond_t *cond,
		char *n_cond)
{
	int err;
	DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
			"%08X:%s:%d TRY cond_broadcast(%s)",
			(unsigned int)pthread_self(), func, line, n_cond);
	err = pthread_cond_broadcast(cond);
	if (err)
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d FAILED cond_broadcast(%s), %d:%s",
				(unsigned int)pthread_self(), func, line, n_cond, err, strerror(err));
#if 0
	else
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d PASSED cond_broadcast(%s)",
				(unsigned int)pthread_self(), func, line, n_cond);
#endif
	return err;
}
static int dhk_cond_signal(const char *func, int line, pthread_cond_t *cond,
		char *n_cond)
{
	int err;
	DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
			"%08X:%s:%d TRY cond_signal(%s)",
			(unsigned int)pthread_self(), func, line, n_cond);
	err = pthread_cond_signal(cond);
	if (err)
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d FAILED cond_signal(%s), %d:%s",
				(unsigned int)pthread_self(), func, line, n_cond, err, strerror(err));
#if 0
	else
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d PASSED cond_signal(%s)",
				(unsigned int)pthread_self(), func, line, n_cond);
#endif
	return err;
}
static int dhk_cond_timedwait(const char *func, int line, pthread_cond_t *cond,
		pthread_mutex_t *mutex, const struct timespec *abstime,
		char *n_cond, char *n_mutex, char *n_abstime)
{
	int err;
	DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
			"%08X:%s:%d TRY cond_timedwait(%s, %s, %s)",
			(unsigned int)pthread_self(), func, line, n_cond, n_mutex, n_abstime);
	err = pthread_cond_timedwait(cond, mutex, abstime);
	if (err)
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d FAILED cond_timedwait(%s, %s, %s), %d:%s",
				(unsigned int)pthread_self(), func, line, n_cond, n_mutex, n_abstime, err, strerror(err));
	else
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d PASSED cond_wait(%s, %s, %s)",
				(unsigned int)pthread_self(), func, line, n_cond, n_mutex, n_abstime);
	return err;
}
static int dhk_cond_wait(const char *func, int line, pthread_cond_t *cond,
		pthread_mutex_t *mutex,
		char *n_cond, char *n_mutex)
{
	int err;
	DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
			"%08X:%s:%d TRY cond_wait(%s, %s)",
			(unsigned int)pthread_self(), func, line, n_cond, n_mutex);
	err = pthread_cond_wait(cond, mutex);
	if (err)
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d FAILED cond_wait(%s, %s), %d:%s",
				(unsigned int)pthread_self(), func, line, n_cond, n_mutex, err, strerror(err));
	else
		DHK_FLOG((LOCK_LOG_COND), DHK_F_BASE"lock.txt",
				"%08X:%s:%d PASSED cond_wait(%s, %s)",
				(unsigned int)pthread_self(), func, line, n_cond, n_mutex);
	return err;
}
#undef LOCK_LOG_COND

#define pthread_mutex_lock(lock) \
	dhk_mutex_lock(__func__, __LINE__, (lock), #lock)
#define pthread_mutex_trylock(lock) \
	dhk_mutex_trylock(__func__, __LINE__, (lock), #lock)
#define pthread_mutex_unlock(lock) \
	dhk_mutex_unlock(__func__, __LINE__, (lock), #lock)
#define pthread_cond_timedwait(cond, lock, time) \
	dhk_cond_timedwait(__func__, __LINE__, (cond), (lock), (time), \
		#cond, #lock, #time)
#define pthread_cond_wait(cond, lock) \
	dhk_cond_wait(__func__, __LINE__, (cond), (lock), #cond, #lock)
#define pthread_cond_signal(cond) \
	dhk_cond_signal(__func__, __LINE__, (cond), #cond)
#define pthread_cond_broadcast(cond) \
	dhk_cond_broadcast(__func__, __LINE__, (cond), #cond)

#endif // DHK_LOCK

#endif // __DHKIM_DEBUG_H_
