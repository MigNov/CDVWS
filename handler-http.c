#define DEBUG_HTTP_HANDLER
#include "cdvws.h"

#ifdef DEBUG_HTTP_HANDLER
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/http-handler] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

void http_host_header(BIO *io, int connected, int error_code, char *mt)
{
	char tmp[TCP_BUF_SIZE] = { 0 };

	if (mt == NULL)
		mt = strdup( "application/octet-stream" );

	strcpy(tmp, "HTTP/1.1 ");
	if (error_code == HTTP_CODE_OK)
		strcat(tmp, "200 OK");
	else
	if (error_code == HTTP_CODE_BAD_REQUEST)
		strcat(tmp, "400 Bad Request");
	else
	if (error_code == HTTP_CODE_UNAUTHORIZED)
		strcat(tmp, "401 Unauthorized");
	else
	if (error_code == HTTP_CODE_FORBIDDEN)
		strcat(tmp, "403 Forbidden");
	else
	if (error_code == HTTP_CODE_NOT_FOUND)
		strcat(tmp, "404 Not Found");
	else
	if (error_code == HTTP_CODE_NOT_ALLOWED)
		strcat(tmp, "405 Not Allowed");

	strcat(tmp, "\nContent-Type: ");
	strcat(tmp, mt);
	strcat(tmp, "\n\n");

	write_common(io, connected, tmp, strlen(tmp));
}

int http_host_put_file(BIO *io, int connected, char *path)
{
	int fd;
	char tmp[TCP_BUF_SIZE] = { 0 };
	char *mt = NULL;
	long size = -1, len = -1;

	if ((size = get_file_size( path )) <= 0)
		return 0;

	mt = get_mime_type(path);
	fd = open(path, O_RDONLY);
	http_host_header(io, connected, HTTP_CODE_OK, mt);
	free(mt);

	while ((len = read(fd, tmp, sizeof(tmp))) > 0)
		write_common(io, connected, tmp, len);
	close(fd);

	return 1;
}

int http_host_unknown(BIO *io, int connected, char *host)
{
	char tmp[TCP_BUF_SIZE] = { 0 };

	snprintf(tmp, sizeof(tmp), "HTTP/1.1 404 Not Found\n\n<html>"
		"<head><title>Server unknown</title></head><body>"
		"<h1>Server unknown</h1>We are sorry but server project "
		"cannot be resolved.<hr />Server is running on CDV "
		"WebServer v%s</body></html>", VERSION);

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

int http_host_page_not_found(BIO *io, int connected, char *path)
{
	char tmp[TCP_BUF_SIZE] = { 0 };
	snprintf(tmp, sizeof(tmp), "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n"
			"<html><head><title>Not Found</title><body><h1>Not Found</h1>"
			"We are sorry but path %s you requested cannot be found.<br /><hr />"
			"<b><u>%s</u></b> running on CDV WebServer v%s. Please contact "
			"site administrator <a target=\"_blank\" href=\"mailto:%s\">%s</a>."
			"<body></html>\n",
				path, VERSION, project_info_get("name"),
				project_info_get("admin_mail"), project_info_get("admin_name"));

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

int process_request_common(SSL *ssl, BIO *io, int connected, struct sockaddr_in client_addr, char *buf, int len)
{
	int i, found;
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

	if (host == NULL)
		return http_host_unknown(io, connected, "unknown");

	DPRINTF("%s: %s for '%s://%s%s', user agent is '%s'\n", __FUNCTION__, method,
		(ssl == NULL) ? "http" : "https", host, path, ua);

	if (find_project_for_web("examples", host) != 0)
		return http_host_unknown(io, connected, host);

	found = 0;

	/* First, try to look for the file in files */
	char *dir = NULL;
	if ((dir = project_info_get("dir_files")) != NULL) {
		char *tmp = project_info_get("dir_root");
		char loc[4096] = { 0 };

		if (tmp != NULL) {
			snprintf(loc, sizeof(loc), "%s/%s%s", tmp, dir, path);
			DPRINTF("%s: Real file is '%s'\n", __FUNCTION__, loc);
			if (access(loc, R_OK) == 0)
				found = (http_host_put_file(io, connected, loc) != 0);
			free(tmp);
		}
		free(dir);
	}

	/* TODO: Implement the CDV-code/script processing to use IDB and other features */

	/* If still not found then return error to response */
	if (found == 0)
		http_host_page_not_found(io, connected, path);

	cleanup();
	return 1;
}

