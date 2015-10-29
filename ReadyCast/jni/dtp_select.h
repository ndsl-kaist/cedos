#ifndef _DTP_SELECT_H_
#define _DTP_SELECT_H_

#include <sys/types.h>

#ifndef __FDS_BITS
#define __FDS_BITS(p) ((p)->fds_bits)
#endif

#ifndef NO_FDSET_EXTENSION

/* fd_set_s : fd_set with scalability
 * NOTE: Default value of FD_SETSIZE is 1024 */
#define FD_SETSIZE_S (64 * FD_SETSIZE)

typedef struct {
    uint64_t fds_bits[FD_SETSIZE_S / (8 * sizeof(uint64_t))];
} __attribute__((packed)) fd_set_s;

//#define fd_set_s fd_set //(-- in case of not working fd_set_s)

#define FD_SET_S(fd, set)	\
	FD_SET((fd) % FD_SETSIZE, &(((fd_set *)(set))[fd / FD_SETSIZE]))

#define FD_CLR_S(fd, set)	\
	FD_CLR((fd) % FD_SETSIZE, &(((fd_set *)(set))[fd / FD_SETSIZE]))

#define FD_ISSET_S(fd, set)	\
	FD_ISSET((fd) % FD_SETSIZE, &(((fd_set *)(set))[fd / FD_SETSIZE]))

#define FD_ZERO_S(set)								\
do {												\
	int i;											\
	for (i = 0; i < FD_SETSIZE_S / FD_SETSIZE; i++)	\
		FD_ZERO(&(((fd_set *)(set))[i]));			\
} while (0)

#else /* NO_FDSET_EXTENSION */

#define fd_set_s fd_set
#define FD_SETSIZE_S FD_SETSIZE
#define FD_SET_S FD_SET
#define FD_CLR_S FD_CLR
#define FD_ISSET_S FD_ISSET
#define FD_ZERO_S FD_ZERO

#endif /* NO_FDSET_EXTENSION */

/* inner functions for dtp_select() */
extern void DTPSetInnerSock(int sock);
extern fd_set_s* DTPGetFdSet();
extern fd_set_s* DTPGetFdRSet();
extern bool DTPIsDTPSocket(int sock);
extern void DTPRegisterSockToGlobalFdSet(int sock);
extern void DTPClearSockFromGlobalFdSet(int sock);

extern void DTPSelectInit(int nfds, fd_set_s* readfds, fd_set_s* writefds);
extern void DTPSelectClear();
extern void DTPSelectEventSet(int lsock, int rw);
extern void DTPSelectEventClr(int lsock, int rw);
extern int DTPSelectCheckAnyPendingEvent(int nfds, fd_set_s* readfds, fd_set_s* writefds);

#endif
