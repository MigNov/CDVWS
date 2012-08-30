#define DEBUG_HTTP_HANDLER
#include "cdvws.h"

#ifdef DEBUG_HTTP_HANDLER
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/http-handler] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

void http_host_header(BIO *io, int connected, int error_code, char *mt, char *cookie, int len)
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

	if (len > 0) {
		char cl[16] = { 0 };
		snprintf(cl, sizeof(cl), "%d", len);

		strcat(tmp, "\nContent-Length: ");
		strcat(tmp, cl);
	}

	if (cookie != NULL) {
		strcat(tmp, "\nSet-Cookie: ");
		strcat(tmp, cookie);
	}

	strcat(tmp, "\n\n");

	write_common(io, connected, tmp, strlen(tmp));
}

int http_host_put_file(BIO *io, int connected, char *path, char *cookies)
{
	int fd;
	char tmp[TCP_BUF_SIZE] = { 0 };
	char *mt = NULL;
	long size = -1, len = -1;

	if ((size = get_file_size( path )) <= 0)
		return 0;

	mt = get_mime_type(path);
	fd = open(path, O_RDONLY);
	http_host_header(io, connected, HTTP_CODE_OK, mt, cookies, 0);
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
		"cannot be resolved.<br />Server address: %s<hr />Server is running on CDV "
		"WebServer v%s</body></html>", host, VERSION);

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

int http_feature_disabled(BIO *io, int connected, char *feature)
{
	char tmp[TCP_BUF_SIZE] = { 0 };

	snprintf(tmp, sizeof(tmp), "HTTP/1.1 403 Forbidden\n\n<html>"
		"<head><title>Feature disabled</title></head><body>"
		"<h1>Feature disabled</h1>We are sorry but feature you requested has been "
		"disabled by system administrator.<br />Feature name: %s<hr />Server is "
		"running on CDV WebServer v%s</body></html>", feature, VERSION);

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
	char *cookie = NULL;

	if ((len > 2) && (buf[len - 1] == '\n')) {
		if ((buf[len - 1] != '>') && (buf[len - 2] != '>')) {
			if (ssl == NULL)
				buf[len - 2] = 0;
			buf[len - 1] = 0;
			if (ssl == NULL) {
				len--;
				buf[len - 1] = 0;
			}
			len--;
		}
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
		else
		if (strncmp(t.tokens[i], "Cookie: ", 8) == 0)
			cookie = strdup(t.tokens[i] + 8);

		DPRINTF("%s: Line %d is '%s'\n", __FUNCTION__, i+1, t.tokens[i]);
	}

	free_tokens(t);

	if (host == NULL)
		return http_host_unknown(io, connected, "unknown");

	DPRINTF("%s: %s for '%s://%s%s', user agent is '%s'\n", __FUNCTION__, method,
		(ssl == NULL) ? "http" : "https", host, path, ua);

	if (((cookie != NULL) && (strstr(cookie, "CDVCookie=") == NULL)) || (cookie == NULL)) {
		char *tmp = NULL;
		char cdvcookie[33] = { 0 };
		char fn[1024] = { 0 };
		char hex[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

		for (i = 0; i < 32; i++) {
			srand(time(NULL) + i);
			cdvcookie[i] = hex[rand() % strlen(hex)];
		}

		snprintf(fn, sizeof(fn), "/tmp/cdvdb-%s", cdvcookie);
		while (access(fn, R_OK) == 0) {
			for (i = 0; i < 32; i++) {
				srand(time(NULL) + i);
				cdvcookie[i] = hex[rand() % strlen(hex)];
			}

			snprintf(fn, sizeof(fn), "/tmp/cdvdb-%s", cdvcookie);
		}

		tmp = (char *)malloc( strlen("CDVCookie=;") + strlen(cdvcookie) );
		sprintf(tmp, "CDVCookie=%s\n", cdvcookie);
		cookie = tmp;

		_cdv_cookie = strdup(cdvcookie);
	}
	else {
		char *tmp = strstr(cookie, "CDVCookie=") + 10;

		for (i = 0; i < strlen(tmp); i++) {
			if ((tmp[i] == ';') || (tmp[i] == '\n'))
				tmp[i] = 0;
		}

		_cdv_cookie = tmp;
	}

	/* First check if user requires shell access */
	if (strncmp(path, "/shell", 6) == 0) {
		/*
		if (_shell_enabled == 0)
			return http_feature_disabled(io, connected, "Internal WebShell");
		*/

		if (strncmp(path, "/shell@", 7) == 0) {
			char *str = strdup( replace(replace(path + 7, "%20", " "), "%2F", "/") );
			http_host_header(io, connected, HTTP_CODE_OK, "text/html", cookie, 0);
			if (strncmp(str, "idb-", 4) == 0)
				process_idb_command(utils_get_time( TIME_CURRENT), io, connected, str + 4);
			else
				process_shell_command(utils_get_time( TIME_CURRENT), io, connected, str, ua, host);
			return 1;
		}
		else {
			char loc[4096] = { 0 };
			char buf[4096] = { 0 };

			getcwd(buf, sizeof(buf));
			if (strcmp(path, "/shell") == 0)
				snprintf(loc, sizeof(loc), "%s%s/shell.html", buf, path);
			else
				snprintf(loc, sizeof(loc), "%s%s", buf, path);

			if (access(loc, R_OK) == 0)
				return http_host_put_file(io, connected, loc, cookie);
		}

		return http_host_unknown(io, connected, "Internal WebShell");
	}

	if (find_project_for_web("examples", host) != 0)
		return http_host_unknown(io, connected, host);

	found = 0;

	if (project_info_get("path_xmlrpc") != NULL) {
		char *xmlrpc = project_info_get("path_xmlrpc");

		DPRINTF("XMLRPC: '%s', PATH: '%s'\n", xmlrpc, path);
		if (strcmp(xmlrpc, path) == 0) {
			char *xml = strstr(buf, "\n<?xml");
			if (xml == NULL) {
				http_host_header(io, connected, HTTP_CODE_BAD_REQUEST, "text/html", NULL, 0);
				return 1;
			}

			*xml++;
			DPRINTF("XML ISSSS: '%s'\n", xml);

			if (xml[strlen(xml) - 1] == '\n')
				xml[strlen(xml) - 1] = 0;

			char *data = xmlrpc_process(xml);

			if (data == NULL)
				data = strdup("<?xml version=\"1.0\"?><methodResponse><fault><value><struct>"
						"<member><name>faultCode</name><value><int>1</int></value></member>"
						"<member><name>faultString</name><value><string>Invalid input data."
						"</string></value></member></struct></value></fault></methodResponse>");

			http_host_header(io, connected, HTTP_CODE_OK, "text/xml", NULL, strlen(data));
			DPRINTF("Returning '%s'\n", data);
			write_common(io, connected, data, strlen(data));
			free(data);
			return 1;
		}
	}

	/* Then try to look for the file in files */
	char *dir = NULL;
	if ((dir = project_info_get("dir_files")) != NULL) {
		char *tmp = project_info_get("dir_root");
		char loc[4096] = { 0 };
		if (tmp != NULL) {
			snprintf(loc, sizeof(loc), "%s/%s%s", tmp, dir, path);
			DPRINTF("%s: Real file is '%s'\n", __FUNCTION__, loc);
			if (access(loc, R_OK) == 0)
				found = (http_host_put_file(io, connected, loc, cookie) != 0);
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

