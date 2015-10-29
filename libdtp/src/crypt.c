#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <openssl/rsa.h>
#include <openssl/hmac.h>
#include "crypt.h"
#include "debug.h"
#include "dhkim_debug.h"

static int spc_devrand_fd = -1;
static int spc_devrand_fd_noblock = -1;
static int spc_devurand_fd = -1;

void spc_make_fd_nonblocking(int fd) 
{
    int flags;

    /* Get flags associated with the descriptor. */
    flags = fcntl(fd, F_GETFL); 
    if (flags == -1) {
	perror("spc_make_fd_nonblocking failed on F_GETFL");
	EXIT(-1, return);
    }
    flags |= O_NONBLOCK;
    /* Now the flags will be the same as before, except with O_NONBLOCK set. */
    if (fcntl(fd, F_SETFL, flags) == -1) {
	perror("spc_make_fd_nonblocking failed on F_SETFL");
	EXIT(-1, return);
    }
}

void spc_rand_init()
{
    spc_devrand_fd = open("/dev/random", O_RDONLY);
    spc_devrand_fd_noblock = open("/dev/random", O_RDONLY);
    spc_devurand_fd = open("/dev/urandom", O_RDONLY);

    if (spc_devrand_fd == -1 || spc_devrand_fd_noblock == -1) {
	perror("spc_rand_init failed to open /dev/random");
	EXIT(-1, return);
    }
    if (spc_devurand_fd == -1) {
	perror("spc_rand_init failed to open /dev/urandom");
	EXIT(-1, return);
    }

    spc_make_fd_nonblocking(spc_devrand_fd_noblock);
}

unsigned char *spc_rand(unsigned char *buf, size_t nbytes) 
{
    ssize_t r;
    unsigned char *where = buf;

    if (spc_devrand_fd == -1 && spc_devrand_fd_noblock == -1 && spc_devurand_fd == -1)
	spc_rand_init();
    while (nbytes) {
	if ((r = read(spc_devurand_fd, where, nbytes)) == -1) {
	    if (errno == EINTR) 
		continue;
	    perror("spc_rand could not read from /dev/urandom");
	    EXIT(-1, );
	}
	where += r;
	nbytes -= r;
    }
    return buf;
}

unsigned char *spc_keygen(unsigned char *buf, size_t nbytes) 
{
    ssize_t r;
    unsigned char *where = buf;

    if (spc_devrand_fd == -1 && spc_devrand_fd_noblock == -1 && spc_devurand_fd == -1)
	spc_rand_init();
    while (nbytes) {
	if ((r = read(spc_devrand_fd_noblock, where, nbytes)) == -1) {
	    if (errno == EINTR) 
		continue;
	    if (errno == EAGAIN) 
		break;
	    perror("spc_rand could not read from /dev/random");
	    printf("%d %d %d\n", spc_devrand_fd, spc_devrand_fd_noblock, spc_devurand_fd);
	    EXIT(-1, );
	}
	where += r;
	nbytes -= r;
    }
    spc_rand(where, nbytes);

    close(spc_devrand_fd);
    close(spc_devrand_fd_noblock);
    close(spc_devurand_fd);
    spc_devrand_fd = -1;
    spc_devrand_fd_noblock = -1;
    spc_devurand_fd = -1;

    return buf;
}
