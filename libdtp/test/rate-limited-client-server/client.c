#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>

#include "../../src/dtp_socket.h"

#define DEFAULT_SERVER     "192.168.0.38:3742"
#define DEFAULT_READ_RATE  (100*1024)
#define DEFAULT_BUF_SIZE   (20*1000*1024)
#define DEFAULT_LOW_BOUND  (20*1024)
#define DEFAULT_HIGH_BOUND (1000*1024)

#define UPDATE_MS          500

#define BUFSIZE            (1024)

int main (int argc, char **argv)
{
	char *server = (char *)malloc(strlen(DEFAULT_SERVER) + 1);
	strcpy(server, DEFAULT_SERVER);
	int port = -1;
	int read_rate = DEFAULT_READ_RATE;
	int buf_size = DEFAULT_BUF_SIZE;
	int low_bound = DEFAULT_LOW_BOUND;
	int high_bound = DEFAULT_HIGH_BOUND;
	int unlimited = 0;

	unsigned char opt; /* opt should be unsigned for compatibility */
	char *buffer;
	unsigned int interval;

	int socket;
	struct hostent *he;
	struct sockaddr_in server_addr;
	struct timeval curTime;

	while ((opt = getopt(argc, argv, "s:r:b:l:h:u")) != 0xFF) {
		switch (opt) {
			/* server domain name and port */
			case 's':
				server = (char *)malloc(strlen(optarg) + 1);
				strcpy(server, optarg);
				break;

			/* read rate (Bps) */
			case 'r':
				read_rate = atoi(optarg);
				break;

			case 'b':
				buf_size = atoi(optarg);
				break;

			case 'l':
				low_bound = atoi(optarg);
				break;

			case 'h':
				high_bound = atoi(optarg);
				break;

			case 'u':
				unlimited = 1;

			default:
				break;
		}
	}
	strtok(server, ":");
	port = atoi(strtok(NULL, ":"));

	if (port < 1024 || port > 65535) {
		printf("invalid port number\n");
		return 0;
	}

	if (read_rate < 0) {
		printf("invalid read rate\n");
		return 0;
	}

	if (!(buffer = (char *)malloc(buf_size))) {
		printf("failed to allocate %d bytes for buffer\n",
				buf_size);
		return 0;
	}

	struct dtp_rcvlobuf rcvlobuf;
	rcvlobuf.low_bound = low_bound;
	rcvlobuf.high_bound = high_bound;

	interval = (1000*1000 * BUFSIZE / read_rate);

	socket = dtp_socket();

	if ((he = gethostbyname(server)) == NULL) {
		printf("DNS failure: %s\n", server);
		return 0;
	}

	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port);
	server_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(server_addr.sin_zero), 8);

	if (dtp_connect(socket, (struct sockaddr *)&server_addr,
				sizeof(struct sockaddr)) == -1) {
		printf("Failed to connect %s\n", server);
		return 0;
	}

	if (dtp_fcntl(socket, DTP_F_SETFL, 
				dtp_fcntl(socket, DTP_F_GETFL, 0) | O_NONBLOCK)) {
		printf("Setting NON-BLOCK failed\n");
		return 0;
	}

	if (dtp_setsockopt(socket, SOL_SOCKET,
				DTP_SO_RCVBUF, &buf_size, sizeof(int))) {
		printf("setting rcv socket buffer as %d failed\n", buf_size);
		return 0;
	}

	if (dtp_setsockopt(socket, SOL_SOCKET,
				DTP_SO_RCVLOBUF, &rcvlobuf, sizeof(struct dtp_rcvlobuf))) {
		printf("setting LB = %d, HB = %d failed\n",
				low_bound, high_bound);
		return 0;
	}

	printf("start receiving\n");
	int rcvd = 0;
	unsigned long timeout = 0;
	unsigned long now = 0;
	printf("             ");
	while (1) {
		int ret = dtp_recv(socket, buffer, 1024, 0);
		int avail = dtp_recv(socket, buffer, buf_size, MSG_PEEK);
		rcvd += ret > 0 ? ret : 0;
		gettimeofday(&curTime, NULL);
		now = (curTime.tv_sec * 1000) + (curTime.tv_usec / 1000);
		if (timeout < now) {
			printf("\r%9.2fkBps, %10d buffered",
					(float)rcvd / (float)(now - timeout + UPDATE_MS),
					avail);
			timeout = now + UPDATE_MS;
			fflush(stdout);
			rcvd = 0;
		}
		if (!unlimited)
			usleep(interval);
	}

	return 0;
}
