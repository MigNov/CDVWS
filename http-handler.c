#define DEBUG_HTTP_HANDLER
#include "cdvws.h"

#ifdef DEBUG_HTTP_HANDLER
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/http-handler] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int http_host_unknown(BIO *io, int connected, char *host)
{
	char tmp[TCP_BUF_SIZE] = { 0 };

	snprintf(tmp, sizeof(tmp), "HTTP/1.1 404 Not Found\n\n<html>"
		"<head><title>Server unknown</title></head><body>"
		"<h1>Server unknown</h1>We are sorry but server project "
		"cannot be resolved.<hr />Server is running on CDV "
		"WebServer v0.0.1</body></html>");

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

int http_host_page_not_found(BIO *io, int connected, char *path)
{
	char tmp[TCP_BUF_SIZE] = { 0 };
	snprintf(tmp, sizeof(tmp), "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n"
			"<html><head><title>Not Found</title><body><h1>Not Found</h1>"
			"We are sorry but path %s you requested cannot be found.<br /><hr />"
			"<b><u>%s</u></b> running on CDV WebServer v0.0.1. Please contact "
			"site administrator <a target=\"_blank\" href=\"mailto:%s\">%s</a>."
			"<body></html>\n",
				path, project_info_get("name"), project_info_get("admin_mail"),
				project_info_get("admin_name"));

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

int process_request_common(SSL *ssl, BIO *io, int connected, struct sockaddr_in client_addr, char *buf, int len)
{
	int i;
	tTokenizer t;
	char *ua = NULL;
	char *host = NULL;
	char *path = NULL;
	char *method = NULL;

	if ((len > 2) && (buf[len - 1] == '\n')) {
		if (ssl == NULL)
			buf[len - 2] = 0;
		buf[len - 1] = 0;
		if (ssl == NULL) {
			len--;
			buf[len - 1] = 0;
		}
		len--;
	}

	DPRINTF("%s: Entering with following data '%s' (%d)\n", __FUNCTION__, buf, len);

	t = tokenize(buf, "\n");
	for (i = 0; i < t.numTokens; i++) {
		if (t.tokens[i][ strlen(t.tokens[i]) - 1 ] == 13)
			t.tokens[i][ strlen(t.tokens[i]) - 1 ] = 0;

		if (i == 0) {
			tTokenizer t2;

			t2 = tokenize(t.tokens[i], " ");
			if ((t2.numTokens == 3) && (strncmp(t2.tokens[2], "HTTP/", 5) == 0)) {
				method = strdup( t2.tokens[0] );
				path = strdup( t2.tokens[1] );
			}
			free_tokens(t2);
		}
		else
		if (strncmp(t.tokens[i], "User-Agent: ", 12) == 0)
			ua = strdup(t.tokens[i] + 12);
		else
		if (strncmp(t.tokens[i], "Host: ", 6) == 0)
			host = strdup(t.tokens[i] + 6);

		DPRINTF("%s: Line %d is '%s'\n", __FUNCTION__, i+1, t.tokens[i]);
	}

	free_tokens(t);

	DPRINTF("%s: %s for '%s://%s%s', user agent is '%s'\n", __FUNCTION__, method,
		(ssl == NULL) ? "http" : "https", host, path, ua);

	if (find_project_for_web("examples", host) != 0)
		return http_host_unknown(io, connected, host);

	http_host_page_not_found(io, connected, path);

	cleanup();
	return 1;
}

