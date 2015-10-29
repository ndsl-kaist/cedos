#ifndef __CRYPT_H__
#define __CRYPT_H__

/* key */
#define RSA_LEN 256
#define PUB_EXP 3 // 3, 17 or 65537
#define MAX_INT 2147483647

/* random symmetric key */
void spc_make_fd_nonblocking(int fd);
void spc_rand_init();
unsigned char *spc_rand(unsigned char *buf, size_t nybtes);
unsigned char *spc_keygen(unsigned char *buf, size_t nbytes);

#endif
