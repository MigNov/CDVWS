#define DEBUG_SOCKETS
#include "cdvws.h"

#ifdef DEBUG_SOCKETS
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/sockets     ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int tcp_listen(int port)
{
	int sock, on = 1;
	struct sockaddr_in server_addr;    

	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		int err = -errno;
		perror("socket");
		close(sock);
		return err;
	}

	if (setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&on,sizeof(on)) == -1) {
		int err = -errno;
		perror("setsockopt");
		close(sock);
		return err;
	}
        
	server_addr.sin_family = AF_INET;         
	server_addr.sin_port = htons(port);
	server_addr.sin_addr.s_addr = INADDR_ANY; 
	bzero(&(server_addr.sin_zero),8); 

	if (bind(sock, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		int err = -errno;
		perror("bind");
		close(sock);
		return err;
	}

	if (listen(sock, 5) == -1) {
		int err = -errno;
		perror("Listen");
		close(sock);
		return err;
	}

	return sock;
}

int socket_has_data(int sfd, long maxtime)
{
	int rc;
	fd_set fds;
	struct timeval timeout;

	timeout.tv_sec = maxtime / 1000000;
	timeout.tv_usec = (maxtime % 1000000);

	FD_ZERO(&fds);
	FD_SET(sfd, &fds);
	rc = select( sizeof(fds), &fds, NULL, NULL, &timeout);
	if (rc == -1)
		return 0;

	return (rc == 1);
}

