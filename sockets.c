#include "cdvws.h"

#ifdef DEBUG_SOCKETS
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/sockets     ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int tcp_listen(int port, int flags)
{
	char ports[6] = { 0 };
	struct addrinfo *ai;
	struct addrinfo hints;
	int nfds = 0;
	int err;

	DPRINTF("%s: Running for port %d with flags %d\n",
		__FUNCTION__, port, flags);

	DPRINTF("%s: Flags = { IPv4 = %s, IPv6 = %s, IPv6 only = %s }\n", __FUNCTION__,
		(flags & TCP_IPV4) ? "true" : "false", (flags & TCP_IPV6) ? "true" : "false",
		(flags & TCP_V6ONLY ? "true" : "false"));

	memset (&hints, 0, sizeof (hints));
	if ((flags & TCP_IPV4) && (flags & TCP_IPV6)) {
		hints.ai_family = PF_UNSPEC;
		DPRINTF("%s: Address family is unspecified\n",
			__FUNCTION__);
	}
	else
	if ((flags & TCP_IPV4) && (!(flags & TCP_IPV6))) {
		hints.ai_family = PF_INET;
		DPRINTF("%s: Address family is IPv4\n",
			__FUNCTION__);
	}
	else
	if ((!(flags & TCP_IPV4)) && (flags & TCP_IPV6)) {
		hints.ai_family = PF_INET6;
		DPRINTF("%s: Address family is IPv6\n",
			__FUNCTION__);
	}
	else {
		hints.ai_family = PF_UNSPEC;
		DPRINTF("%s: Address family is unspecified\n",
			__FUNCTION__);
	}

	hints.ai_flags = AI_PASSIVE;
	hints.ai_socktype = SOCK_STREAM;

	snprintf(ports, sizeof(ports), "%d", port);
	err = getaddrinfo(NULL, ports, &hints, &ai);
	if (err != 0) {
		int err = -errno;
		perror("getaddrinfo");
		return err;
	}

	struct addrinfo *runp = ai;
	while (runp != NULL) {
		++nfds;
		runp = runp->ai_next;
	}

	DPRINTF("%s: Number of possible descriptors is %d (PID #%d)\n", __FUNCTION__, nfds, getpid());

	_tcp_sock_fds = (struct pollfd *)utils_alloc( "tcp_listen", nfds * sizeof(struct pollfd *) );
	memset(_tcp_sock_fds, 0, nfds * sizeof(struct pollfd *) );

	nfds = 0;
	for (runp = ai; runp != NULL; runp = runp->ai_next) {
		int on = 1;
		char lip[20] = { 0 };
		(void) getnameinfo (runp->ai_addr, runp->ai_addrlen, lip,
			sizeof(lip), NULL, 0, NI_NUMERICHOST);

		DPRINTF("%s: Trying to bind to %s (family is IPv%d, sock type %d)\n", __FUNCTION__, lip,
			runp->ai_family == AF_INET ? 4 : 6, runp->ai_socktype);

		_tcp_sock_fds[nfds].fd = socket(runp->ai_family, runp->ai_socktype, runp->ai_protocol);
		if (_tcp_sock_fds[nfds].fd == -1) {
			perror("socket");
			close(_tcp_sock_fds[nfds].fd);
			break;
		}

#ifdef IPV6_V6ONLY
		if ((flags & TCP_V6ONLY) && (runp->ai_family == PF_INET6)) {
			int on = 1;

			if (setsockopt(_tcp_sock_fds[nfds].fd, IPPROTO_IPV6,IPV6_V6ONLY, &on, sizeof(on)) < 0)
				perror("setsockopt(IPV6_V6ONLY)");
			else
				DPRINTF("%s: Socket is accepting only IPv6 connections\n", __FUNCTION__);
		}
#endif

		_tcp_sock_fds[nfds].events = POLLIN;
		if (setsockopt(_tcp_sock_fds[nfds].fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) == -1) {
			perror("setsockopt");
			close(_tcp_sock_fds[nfds].fd);
			break;
		}

		if (bind(_tcp_sock_fds[nfds].fd, (struct sockaddr *)runp->ai_addr, runp->ai_addrlen) == -1) {
			perror("bind");
			close(_tcp_sock_fds[nfds].fd);
			break;
		}

		if (listen(_tcp_sock_fds[nfds].fd, 5) == -1) {
			perror("listen");
			close(_tcp_sock_fds[nfds].fd);
			break;
		}

		DPRINTF("%s: Descriptor #%d created and listening (family %d, socktype %d, protocol %d, len %d)\n",
			__FUNCTION__, nfds, runp->ai_family, runp->ai_socktype,
			runp->ai_protocol, runp->ai_addrlen);

		++nfds;
	}
	freeaddrinfo(ai);

	if (nfds == 0) {
		DPRINTF("%s: No socket created\n", __FUNCTION__);
		return -EINVAL;
	}

	DPRINTF("%s: %d sockets created\n", __FUNCTION__, nfds);
	_tcp_sock_fds_num = nfds;
	return nfds;
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

int run_server(int port, char *pk, char *pub, char *root_key, int flags)
{
	int i, nfds;
	if ((port < 1) || (port > 65535))
		return -EINVAL;

	signal(SIGUSR1, socksig);

	/* Applicable only for PK and PUB set, i.e. SSL connection */
	if ((pk != NULL) && (pub != NULL) && (root_key != NULL)) {
		int fd[2];
		char buf[16] = { 0 };

		pipe(fd);
		if (fork() == 0) {
			SSL_CTX *ctx = NULL;

			close(fd[0]);
			snprintf(buf, sizeof(buf), "%d", getpid());
			write(fd[1], buf, strlen(buf));

			DPRINTF("%s: Opening SSL server on port %d\n",
				__FUNCTION__, port);
			ctx = init_ssl_layer(pk, pub, root_key);

			nfds = tcp_listen(port, flags);
			if (nfds == 0) {
				write(fd[1], "ERR", 3);
				close(fd[1]);
				_exit(1);
			}

			if (ctx != NULL)
				accept_loop(ctx, nfds, process_request_common);

			SSL_CTX_free(ctx);
			DPRINTF("%s: Closing SSL socket\n", __FUNCTION__);
			close(fd[1]);
			for (i = 0; i < nfds; i++)
				close(_tcp_sock_fds[i].fd);
			exit(0);
		}

		/* Try to wait to spawn server, if it fails we will know */
		usleep(50000);

		close(fd[1]);
		read(fd[0], buf, sizeof(buf));
		close(fd[0]);
		if (strstr(buf, "ERR") == NULL) {
			char tmp[1024] = { 0 };

			snprintf(tmp, sizeof(tmp), "SSL Server on port %d", port);
			utils_pid_add( atoi(buf), tmp );
		}
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
			close(fd[0]);
			snprintf(buf, sizeof(buf), "%d", getpid());
			write(fd[1], buf, strlen(buf));

			DPRINTF("%s: Opening server on port %d\n", __FUNCTION__, port);
			nfds = tcp_listen(port, flags);
			if (nfds > 0)
				accept_loop(NULL, nfds, process_request_common);

			DPRINTF("%s: Closing socket\n", __FUNCTION__);
			close(fd[1]);
			for (i = 0; i < nfds; i++)
				close(_tcp_sock_fds[i].fd);
			exit(0);
		}

		/* Try to wait to spawn server, if it fails we will know */
		usleep(50000);

		close(fd[1]);
		read(fd[0], buf, sizeof(buf));
		close(fd[0]);
		if (strstr(buf, "ERR") == NULL) {
			char tmp[1024] = { 0 };

			snprintf(tmp, sizeof(tmp), "TCP Server on port %d", port);
			utils_pid_add( atoi(buf), tmp );
		}
		else
			waitpid( atoi(buf), NULL, 0 );

		utils_pid_wait_all();
	}

	return 0;
}

