#include "cdvws.h"

#ifdef DEBUG_SSL
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/ssl-handler ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

static int s_server_session_id_context = 1;

SSL_CTX *initialize_ctx(char *private_key, char *public_key, char *root_cert)
{
	SSL_CTX *ctx;

	SSL_library_init();
	signal(SIGPIPE, SIG_IGN);

	//ctx = SSL_CTX_new( SSLv23_method() );
	ctx = SSL_CTX_new( TLSv1_server_method() );

	SSL_CTX_use_certificate_file(ctx, public_key, SSL_FILETYPE_PEM);
	SSL_CTX_use_PrivateKey_file(ctx, private_key, SSL_FILETYPE_PEM);

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);
	SSL_CTX_load_verify_locations(ctx, root_cert, 0);

#if (OPENSSL_VERSION_NUMBER < 0x00905100L)
	SSL_CTX_set_verify_depth(ctx, 1);
#endif

	SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, 0);
	return ctx;
}

SSL_CTX *init_ssl_layer(char *private_key, char *public_key, char *root_key)
{
	SSL_CTX *ctx;

	ctx = initialize_ctx(private_key, public_key, root_key);

	SSL_CTX_set_session_id_context(ctx,
		(void*)&s_server_session_id_context,
		sizeof(s_server_session_id_context));

	return ctx;
}

int process_request_ssl(SSL *ssl, int s, struct sockaddr_in client_addr, tProcessRequest req)
{
	char buf[TCP_BUF_SIZE] = { 0 };
	BIO *io,*ssl_bio;
	int r, len = -1;

	io=BIO_new(BIO_f_buffer());
	ssl_bio=BIO_new(BIO_f_ssl());
	BIO_set_ssl(ssl_bio,ssl,BIO_CLOSE);
	BIO_push(io,ssl_bio);

	while (1) {
		memset(buf, 0, sizeof(buf));

		r = BIO_gets(io, buf, sizeof(buf)-1);
		switch(SSL_get_error(ssl, r)){
			case SSL_ERROR_NONE:
				len = r;
				break;
			case SSL_ERROR_ZERO_RETURN:
				DPRINTF("%s: SSL Error is zero return\n", __FUNCTION__);
				break;
			case SSL_ERROR_WANT_READ:
				DPRINTF("%s: SSL Error is SSL_ERROR_WANT_READ\n", __FUNCTION__);
				break;
			case SSL_ERROR_WANT_WRITE:
				DPRINTF("%s: SSL Error is SSL_ERROR_WANT_WRITE\n", __FUNCTION__);
				break;
			case SSL_ERROR_WANT_CONNECT:
				DPRINTF("%s: SSL Error is SSL_ERROR_WANT_CONNECT\n", __FUNCTION__);
				break;
			case SSL_ERROR_WANT_ACCEPT:
				DPRINTF("%s: SSL Error is SSL_ERROR_WANT_ACCEPT\n", __FUNCTION__);
				break;
			case SSL_ERROR_WANT_X509_LOOKUP:
				DPRINTF("%s: SSL Error is SSL_ERROR_WANT_X509_LOOKUP\n", __FUNCTION__);
				break;
/*
			case SSL_ERROR_SYSCALL:
				DPRINTF("%s: SSL Error is SSL_ERROR_SYSCALL\n", __FUNCTION__);
				break;
			case SSL_ERROR_SSL:
				DPRINTF("%s: SSL Error is SSL_ERROR_SSL\n", __FUNCTION__);
				break;
*/
			default:
				DPRINTF("%s: SSL Error is default\n", __FUNCTION__);
				break;
		}

		if(!strcmp(buf,"\r\n") ||
			!strcmp(buf,"\n"))
				break;

		if (strlen(buf) == 0)
			return 1;

		len = r;
		/* Set buffer */
		if ((len >= 0) && (_tcp_total + len < sizeof(_tcp_buf))) {
			memcpy(_tcp_buf + _tcp_total, buf, len);
			_tcp_total += len;
		}
	}

	if ((_tcp_total > 0) && (req(ssl, io, -1, client_addr, _tcp_buf, _tcp_total) == 1)) {
		r = SSL_shutdown(ssl);
		if(!r) {
			shutdown(s, 1);
			r = SSL_shutdown(ssl);
		}
		SSL_free(ssl);
		close(s);
		return 1;
	}

	return 0;
}

int process_request_plain(int s, struct sockaddr_in client_addr, tProcessRequest req)
{
	int len;
	char buf[TCP_BUF_SIZE] = { 0 };

	DPRINTF("%s: Getting data from fd #%d...\n", __FUNCTION__, s);

	while (1) {
		memset(buf, 0, sizeof(buf));

		if (socket_has_data(s, 50000) != 1) {
			DPRINTF("%s: No more data on fd #%d\n", __FUNCTION__, s);
			break;
		}

		len = recv(s, buf, sizeof(buf), 0);
		if (len == 0) {
			DPRINTF("%s: No data available on fd #%d\n", __FUNCTION__, s);
			break;
		}

		DPRINTF("%s: Got %d bytes from fd #%d\n", __FUNCTION__, len, s);

		if (buf[len] != '>')
			buf[len] = 0;
		if (len == 0) {
			DPRINTF("%s: No data available on fd #%d\n", __FUNCTION__, s);
			break;
		}

		if (_tcp_total + len < sizeof(_tcp_buf)) {
			memcpy(_tcp_buf + _tcp_total, buf, len);
			_tcp_total += len;
		}
	}

	if ((_tcp_total > 0) && (req(NULL, NULL, s, client_addr, _tcp_buf, _tcp_total) == 1)) {
		DPRINTF("%s: Processed %d total bytes\n", __FUNCTION__, _tcp_total);
		close(s);
		return 1;
	}

	DPRINTF("%s: Done\n", __FUNCTION__);
	return 0;
}

int process_request(SSL *ssl, int s, struct sockaddr_in client_addr, tProcessRequest req)
{
	int ret;

	DPRINTF("%s: Establishing %s connection\n", __FUNCTION__, (ssl != NULL)
		? "secure" : "insecure");

	memset(_tcp_buf, 0, sizeof(_tcp_buf));
	_tcp_total = 0;

	if (ssl == NULL)
		ret = process_request_plain(s, client_addr, req);
	else
		ret = process_request_ssl(ssl, s, client_addr, req);

	DPRINTF("%s: Request return value is %d\n", __FUNCTION__, ret);
	return ret;
}

int write_common(BIO *io, int sock, char *data, int len)
{
	int ret;
	
	if (io != NULL) {
		ret = BIO_write(io, data, len);
		if (BIO_flush(io) < 1)
			ret = 0;
	}
	else
		ret = write(sock, data, len);
		
	return ret;
}

void shutdown_common(SSL *ssl, int s, int reason)
{
	if (ssl != NULL) {
		int r;
		r = SSL_shutdown(ssl);
		if(!r){
			shutdown(s, reason);
			r = SSL_shutdown(ssl);
		}
		SSL_free(ssl);
	}
	else
		shutdown(s, reason);
		
	close(s);
}

void tcpsig(int sig)
{
	if (sig == SIGUSR1)
		_sockets_done = 1;
}

int accept_loop(SSL_CTX *ctx, int sock, tProcessRequest req)
{
	int s;
	BIO *sbio;
	SSL *ssl = NULL;
	pid_t cpid;
	struct sockaddr_in client_addr;    
	socklen_t sin_size;

	_sockets_done = 0;

	signal(SIGUSR1, tcpsig);
	DPRINTF("%s: Signal handler set\n", __FUNCTION__);

	while (!_sockets_done) {
		sin_size = sizeof(struct sockaddr_in);

		if (socket_has_data(sock, 50000)) {
			if((s = accept(sock, (struct sockaddr *)&client_addr, &sin_size)) < 0)
				return -EINVAL;

			if ((cpid = fork()) == 0) {
				DPRINTF("%s: Accepted new %s connection from %s\n", __FUNCTION__,
					(ctx == NULL) ? "insecure" : "secure", inet_ntoa(client_addr.sin_addr));
				if (ctx != NULL) {
					sbio=BIO_new_socket(s,BIO_NOCLOSE);
					ssl=SSL_new(ctx);
					SSL_set_bio(ssl,sbio,sbio);

					SSL_accept(ssl);
				}

				if (process_request(ssl, s, client_addr, req) == 1) {
					shutdown_common(ssl, s, SHUT_RDWR);
					close(sock);
					close(s);
					return 1;
				}
			}
			else {
				wait(NULL);
				close(s);
			}
		}
	}

	shutdown_common(ssl, sock, SHUT_RDWR);
	close(sock);

	return 0;
}

