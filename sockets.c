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

void socksig(int sig)
{
	DPRINTF("%s: Signal %d caught\n", __FUNCTION__, sig);

	if (sig == SIGUSR1) {
		utils_pid_signal_all(SIGUSR1);
		utils_pid_wait_all();
	}
}

int run_server(int port, char *pk, char *pub, char *root_key)
{
	if ((port < 1) || (port > 65535))
		return -EINVAL;

	signal(SIGUSR1, socksig);

	/* Applicable only for PK and PUB set, i.e. SSL connection */
	if ((pk != NULL) && (pub != NULL) && (root_key != NULL)) {
		int fd[2];
		char buf[16] = { 0 };

		pipe(fd);
		if (fork() == 0) {
			int sock;
			SSL_CTX *ctx = NULL;

			close(fd[0]);
			snprintf(buf, sizeof(buf), "%d", getpid());
			write(fd[1], buf, strlen(buf));

			DPRINTF("%s: Opening SSL server on port %d\n",
				__FUNCTION__, port);
			ctx = init_ssl_layer(pk, pub, root_key);
			if ((sock = tcp_listen(port)) == -1) {
				write(fd[1], "ERR", 3);
				close(fd[1]);
				_exit(1);
			}

			if (ctx != NULL)
				accept_loop(ctx, sock, process_request_common);

			SSL_CTX_free(ctx);
			DPRINTF("%s: Closing SSL socket\n", __FUNCTION__);
			close(fd[1]);
			close(sock);
			exit(0);
		}

		/* Try to wait to spawn server, if it fails we will know */
		usleep(50000);

		close(fd[1]);
		read(fd[0], buf, sizeof(buf));
		close(fd[0]);
		if (strstr(buf, "ERR") == NULL)
			utils_pid_add( atoi(buf) );
		else
			waitpid( atoi(buf), NULL, 0 );

		utils_pid_wait_all();
	}

	/* Applicable only for PK and PUB unset, i.e. non-SSL connection */
	if ((pk == NULL) && (pub == NULL) && (root_key == NULL)) {
		int fd[2];
		char buf[16] = { 0 };

		pipe(fd);
		if (fork() == 0) {
			int sock;

			close(fd[0]);
			snprintf(buf, sizeof(buf), "%d", getpid());
			write(fd[1], buf, strlen(buf));

			DPRINTF("%s: Opening server on port %d\n", __FUNCTION__, port);
			if ((sock = tcp_listen(port)) == -1) {
				write(fd[1], "ERR", 3);
				close(fd[1]);
				_exit(1);
			}

			accept_loop(NULL, sock, process_request_common);

			DPRINTF("%s: Closing socket\n", __FUNCTION__);
			close(fd[1]);
			close(sock);
			exit(0);
		}

		/* Try to wait to spawn server, if it fails we will know */
		usleep(50000);

		close(fd[1]);
		read(fd[0], buf, sizeof(buf));
		close(fd[0]);
		if (strstr(buf, "ERR") == NULL)
			utils_pid_add( atoi(buf) );
		else
			waitpid( atoi(buf), NULL, 0 );

		utils_pid_wait_all();
	}

	return 0;
}

