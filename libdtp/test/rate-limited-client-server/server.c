#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <time.h>
#include <errno.h>

#include "../../src/dtp_socket.h"

#define DEFAULT_W_RATE     (1000*1024)
#define DEFAULT_C_RATE     (500*1024)
#define DEFAULT_PORT       3742

#define IS_WIFI(ip) \
	((ip & 0x0000F88F) == 0x0000F88F || (ip & 0x0000A8C0) == 0x0000A8C0)

#define BUFSIZE            (1024)
#define BACKLOG            10

int main (int argc, char **argv)
{
	char opt;
	int sockfd, new_fd;
	struct sockaddr_in my_addr;    /* my address information */
	struct sockaddr_in their_addr; /* connector's address information */
	int sin_size;

	struct timeval curTime;
	int milli;
	char timeString[80];

	int w_rate = DEFAULT_W_RATE;
	int c_rate = DEFAULT_C_RATE;
	int port = DEFAULT_PORT;
	int unlimited = 0;

	char buffer[BUFSIZE] = {0};

	unsigned int w_interval;
	unsigned int c_interval;

	int isWifi = -1;

	while ((opt = getopt(argc, argv, "w:c:p:u")) != -1)
		switch (opt) {
			/* wifi send rate (Bps)*/
			case 'w':
				w_rate = atoi(optarg);
				break;
			/* cellular send rate (Bps)*/
			case 'c':
				c_rate = atoi(optarg);
				break;

			case 'p':
				port = atoi(optarg);
				break;

			case 'u':
				unlimited = 1;

			default:
				break;
		}

	if (port < 1024 || port > 65535) {
		printf("invalid port number\n");
		return 0;
	}

	if (w_rate < 0 || c_rate < 0) {
		printf("invalid rate\n");
		return 0;
	}

	w_interval = (1000*1000 * BUFSIZE / w_rate);
	c_interval = (1000*1000 * BUFSIZE / c_rate);

	sockfd = dtp_socket();

	my_addr.sin_family = AF_INET;         /* host byte order */
	my_addr.sin_port = htons(port);     /* short, network byte order */
	my_addr.sin_addr.s_addr = INADDR_ANY; /* auto-fill with my IP */
	bzero(&(my_addr.sin_zero), 8);        /* zero the rest of the struct */

	if (dtp_bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr))
			== -1) {
		perror("bind");
		exit(1);
	}

	if (dtp_listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}

	while(1) {  /* main accept() loop */
		printf("start accepting\n");
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = dtp_accept(sockfd, (struct sockaddr *)&their_addr,
						(socklen_t*)&sin_size)) == -1) {
			perror("accept");
			continue;
		}
		gettimeofday(&curTime, NULL);
		milli = curTime.tv_usec / 1000;
		strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));
		printf("(%s.%03d)server: got connection from %u.%u.%u.%u\n"
				, buffer, milli,
				(their_addr.sin_addr.s_addr & 0x000000ff),
				(their_addr.sin_addr.s_addr & 0x0000ff00) >> 8,
				(their_addr.sin_addr.s_addr & 0x00ff0000) >> 16,
				(their_addr.sin_addr.s_addr & 0xff000000) >> 24);
		fflush(stdout);
		//		if (!fork()) { /* this is the child process */
		if (dtp_fcntl(new_fd, DTP_F_SETFL, 
					dtp_fcntl(new_fd, DTP_F_GETFL, 0) | O_NONBLOCK)) {
			printf("Setting NON-BLOCK failed\n");
			return 0;
		}
		while (1) {
			int state;
			int ret;
			
			dtp_getpeername(new_fd, (struct sockaddr *)&their_addr,
					(socklen_t*)&sin_size);
			if ((ret = dtp_send(new_fd, buffer, BUFSIZE, 0)) == -1)
				if (errno != EAGAIN) {
					perror("dtp_send");
					break;
				}
			if (state != IS_WIFI(their_addr.sin_addr.s_addr)) {
				state = IS_WIFI(their_addr.sin_addr.s_addr);
				gettimeofday(&curTime, NULL);
				milli = curTime.tv_usec / 1000;
				strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", localtime(&curTime.tv_sec));
				printf("(%s.%03d)server: Interface changed %u.%u.%u.%u (%s)\n"
						, buffer, milli,
						(their_addr.sin_addr.s_addr & 0x000000ff),
						(their_addr.sin_addr.s_addr & 0x0000ff00) >> 8,
						(their_addr.sin_addr.s_addr & 0x00ff0000) >> 16,
						(their_addr.sin_addr.s_addr & 0xff000000) >> 24,
						state == 1 ? "WIFI" : "CELLULAR");
				fflush(stdout);
			}
			if (!unlimited)
				usleep(IS_WIFI(their_addr.sin_addr.s_addr) ?
						w_interval : c_interval);
		}
		dtp_close(new_fd);
//		exit(0);
//		}
//		else {
		//	dtp_close(new_fd);  /* parent doesn't need this */
//		}

//		while(waitpid(-1,NULL,WNOHANG) > 0); /* clean up child processes */
	}
}

