#include "cdvws.h"

//#define DEBUG_MSG_WAIT

#ifdef DEBUG_HTTP_HANDLER
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/http-handler] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

void http_host_header(BIO *io, int connected, int error_code, char *host, char *mt, char *cookie, char *realm, int len)
{
	char tmp[TCP_BUF_SIZE_SMALL] = { 0 };

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
	strcat(tmp, "\nHost: ");
	strcat(tmp, host);

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

	if (realm != NULL) {
/*
		if (strcmp(realm, "<NEGOTIATE>") == 0)
			strcat(tmp, "\nWWW-Authenticate: Negotiate");
		else {
			strcat(tmp, "\nWWW-Authenticate: Basic realm=\"");
			strcat(tmp, realm);
			strcat(tmp, "\"");
		}
*/
		strcat(tmp, "\nWWW-Authenticate: Negotiate");
		strcat(tmp, "\nWWW-Authenticate: Basic realm=\"");
		strcat(tmp, realm);
		strcat(tmp, "\"");
	}

	strcat(tmp, "\n\n");

	DPRINTF("Sending header:\n%s\n", tmp);

	write_common(io, connected, tmp, strlen(tmp));
}

int http_host_put_file(BIO *io, int connected, char *host, char *path, char *cookies)
{
	int fd;
	char tmp[TCP_BUF_SIZE_SMALL] = { 0 };
	char *mt = NULL;
	long size = -1, len = -1;

	if ((size = get_file_size( path )) <= 0)
		return 0;

	mt = get_mime_type(path);
	fd = open(path, O_RDONLY);
	http_host_header(io, connected, HTTP_CODE_OK, host, mt, cookies, myRealm, 0);
	mt = utils_free("host.http_host_put_file.mt", mt);

	while ((len = read(fd, tmp, sizeof(tmp))) > 0)
		write_common(io, connected, tmp, len);
	close(fd);

	return 1;
}

int http_host_unknown(BIO *io, int connected, char *host)
{
	char tmp[TCP_BUF_SIZE_SMALL] = { 0 };
	char tmpb[TCP_BUF_SIZE_SMALL] = { 0 };

	snprintf(tmpb, sizeof(tmpb), "<html><head><title>Server unknown</title></head><body>"
		"<h1>Server unknown</h1>We are sorry but server project "
		"cannot be resolved.<br />Server address: %s<hr />Server is running on CDV "
		"WebServer v%s</body></html>", host, VERSION);

	snprintf(tmp, sizeof(tmp), "HTTP/1.1 404 Not Found\nContent-Type: text/html\n"
		"Content-Length: %d\n\n%s", (int)strlen(tmpb), tmpb);

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

int http_feature_disabled(BIO *io, int connected, char *feature)
{
	char tmp[TCP_BUF_SIZE_SMALL] = { 0 };
	char tmpb[TCP_BUF_SIZE_SMALL] = { 0 };

	snprintf(tmpb, sizeof(tmpb), "<html><head><title>Feature disabled</title></head><body>"
		"<h1>Feature disabled</h1>We are sorry but feature you requested has been "
		"disabled by system administrator.<br />Feature name: %s<hr />Server is "
		"running on CDV WebServer v%s</body></html>", feature, VERSION);

	snprintf(tmp, sizeof(tmp), "HTTP/1.1 403 Forbidden\nContent-Type: text/html\n"
		"Content-Length: %d\n\n%s", (int)strlen(tmpb), tmpb);

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

int http_host_page_not_found(BIO *io, int connected, char *path)
{
	char tmp[TCP_BUF_SIZE_SMALL] = { 0 };
	char tmpb[TCP_BUF_SIZE_SMALL] = { 0 };

	snprintf(tmpb, sizeof(tmpb), "<html><head><title>Not Found</title><body><h1>Not Found</h1>"
			"We are sorry but path %s you requested cannot be found.<br /><hr />"
			"<b><u>%s</u></b> running on CDV WebServer v%s. Please contact "
			"site administrator <a target=\"_blank\" href=\"mailto:%s\">%s</a>."
			"<body></html>\n",
				path, project_info_get("name"), VERSION,
				project_info_get("admin_mail"), project_info_get("admin_name"));

	snprintf(tmp, sizeof(tmp), "HTTP/1.1 404 Not Found\nContent-Type: text/html\n"
			"Content-Length: %d\n\n%s", (int)strlen(tmpb), tmpb);

	write_common(io, connected, tmp, strlen(tmp));
	return 1;
}

void http_parse_data(char *data, int tp)
{
	tTokenizer t, t2;
	char *name, *value;
	int i;

	if (data == NULL)
		return;

	t = tokenize(data, "&");
	for (i = 0; i < t.numTokens; i++) {
		name = NULL;
		value = NULL;
		t2 = tokenize(t.tokens[i], "=");
		if (t2.numTokens == 2) {
			name = strdup( t2.tokens[0] );
			value = strdup( t2.tokens[1] );
		}
		free_tokens(t2);

		if (strstr(name, "[]") != NULL) {
			int idParent = -1;

			name[ strlen(name) - 2 ] = 0;

			if ((idParent = variable_lookup_name_idx(name, (tp == TYPE_QPOST) ? "post" : "get", idParent)) == -1)
				idParent = variable_add(name, NULL, tp, -1, TYPE_ARRAY);

			variable_add(NULL, value, tp, idParent, gettype(value));
		}
		else
		if (strstr(name, "[") != NULL) {
			int idParent = -1;
			char *subname = strstr(name, "[") + 1;

			subname[ strlen(subname) - 1 ] = 0;
			name[strlen(name) - strlen(subname) - 1] = 0;

			if ((idParent = variable_lookup_name_idx(name, (tp == TYPE_QPOST) ? "post" : "get", idParent)) == -1)
				idParent = variable_add(name, NULL, tp, -1, TYPE_STRUCT);

			variable_add(subname, value, tp, idParent, gettype(value));
		}
		else
			variable_add(name, value, tp, -1, gettype(value));

		name = utils_free("http.http_parse_data.name", name);
		value = utils_free("http.http_parse_data.value", value);
	}
	free_tokens(t);
}

void http_parse_data_getpost(char *get, char *post)
{
	http_parse_data(get, TYPE_QGET);
	http_parse_data(post, TYPE_QPOST);

	variable_dump();
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
	char *params_get = NULL;
	char *params_post = NULL;

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

				if (strstr(path, "?") != NULL) {
					params_get = strdup( strstr(path, "?") + 1 );

					path[ strlen(path) - strlen(params_get) - 1 ] = 0;
				}
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
		if (strlen(t.tokens[i]) == 0) {
			int j;

			for (j = i + 1; j < t.numTokens; j++) {
				char *tmp = cdvStringAppend(params_post, t.tokens[j]);

				if (tmp != NULL) {
					params_post = strdup(tmp);
					tmp = utils_free("http.cookies", tmp);
				}
			}
		}

		DPRINTF("%s: Line %d is '%s'\n", __FUNCTION__, i+1, t.tokens[i]);
	}

	free_tokens(t);

	if (host == NULL) {
		method = utils_free("http.method", method);
		path = utils_free("http.path", path);
		return http_host_unknown(io, connected, "unknown");
	}

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

		tmp = (char *)utils_alloc( "handler-http.process_request_common.cdvcookie", strlen("CDVCookie=;") + strlen(cdvcookie) );
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
		if (_shell_enabled == 0) {
			method = utils_free("http.method", method);
			path = utils_free("http.path", path);
			cleanup();
			return http_feature_disabled(io, connected, "Internal WebShell");
		}

		if (strncmp(path, "/shell@", 7) == 0) {
			char *str = strdup( replace(replace(path + 7, "%20", " "), "%2F", "/") );
			http_host_header(io, connected, HTTP_CODE_OK, host, "text/html", cookie, myRealm, 0);
			if (strncmp(str, "idb-", 4) == 0)
				process_idb_command(utils_get_time( TIME_CURRENT), io, connected, str + 4);
			else
				process_shell_command(utils_get_time( TIME_CURRENT), io, connected, str, ua, host);
			method = utils_free("http.method", method);
			path = utils_free("http.path", path);
			cleanup();
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

			if (access(loc, R_OK) == 0) {
				method = utils_free("http.method", method);
				path = utils_free("http.path", path);
				return http_host_put_file(io, connected, host, loc, cookie);
			}
		}

		method = utils_free("http.method", method);
		path = utils_free("http.path", path);
		cleanup();
		return http_host_unknown(io, connected, "Internal WebShell");
	}

	if (find_project_for_web("examples", host) != 0) {
		method = utils_free("http.method", method);
		path = utils_free("http.path", path);
		cleanup();
		return http_host_unknown(io, connected, host);
	}

	DPRINTF("Host %s is being served by PID #%d\n", host, getpid());

	#ifdef DEBUG_MSG_WAIT
	DPRINTF("***** To debug this process please run: gdb -p %d *****\n", getpid());
	sleep(10);
	#endif

	found = 0;
	if (project_info_get("path_xmlrpc") != NULL) {
		char *xmlrpc = project_info_get("path_xmlrpc");

		if (strcmp(xmlrpc, path) == 0) {
			char *xml = strstr(buf, "\n<?xml");
			if (xml == NULL) {
				http_host_header(io, connected, HTTP_CODE_BAD_REQUEST, host, "text/html", NULL, myRealm, 0);
				method = utils_free("http.method", method);
				path = utils_free("http.path", path);
				cleanup();
				return 1;
			}

			*xml++;

			/* Fix because of issues */
			if (xml[strlen(xml) - 1] == '\n')
				xml[strlen(xml) - 1] = 0;

			char *data = xmlrpc_process(xml);

			if (data == NULL)
				data = strdup("<?xml version=\"1.0\"?><methodResponse><fault><value><struct>"
						"<member><name>faultCode</name><value><int>1</int></value></member>"
						"<member><name>faultString</name><value><string>Invalid input data."
						"</string></value></member></struct></value></fault></methodResponse>");

			http_host_header(io, connected, HTTP_CODE_OK, host, "text/xml", NULL, myRealm, strlen(data));
			DPRINTF("Returning XMLRPC result: '%s'\n", data);
			write_common(io, connected, data, strlen(data));
			data = utils_free("http.data" ,data);
			path = utils_free("http.path", path);
			method = utils_free("http.method", method);
			cleanup();
			return 1;
		}
	}

	#ifdef USE_KERBEROS
	if (project_info_get("kerberos_secure_path") != NULL) {
		char *ksd = project_info_get("kerberos_secure_path");
		if (strncmp(path, ksd, strlen(ksd)) == 0) {
			char *c = http_get_authorization(buf, host, project_info_get("kerberos_keytab"), project_info_get("kerberos_realm"));
			if (c == NULL) {
				char *err = "<h1>Unauthorized!</h1>";

				http_host_header(io, connected, HTTP_CODE_UNAUTHORIZED, host, "text/html", NULL,
					project_info_get("kerberos_realm_fallback"), strlen(err));
				write_common(io, connected, err, strlen(err));
				method = utils_free("http.method", method);
				path = utils_free("http.path", path);
				cleanup();
				return 1;
			}
			else {
				DPRINTF("%s: User is %s. Setting USERNAME variable\n", __FUNCTION__, c);

				variable_add_fl("USERNAME", c, TYPE_MODAUTH, -1, TYPE_STRING, 1);
			}
		}
	}
	#endif

	http_parse_data_getpost(params_get, params_post);

	/* Then try to look for the file in files */
	char *dir = NULL;
	if ((dir = project_info_get("dir_files")) != NULL) {
		char *tmp = project_info_get("dir_root");
		char loc[4096] = { 0 };
		if (tmp != NULL) {
			snprintf(loc, sizeof(loc), "%s/%s%s", tmp, dir, path);
			DPRINTF("%s: Real file is '%s'\n", __FUNCTION__, loc);
			if (access(loc, R_OK) == 0)
				found = (http_host_put_file(io, connected, host, loc, cookie) != 0);
			tmp = utils_free("http.tmp", tmp);
		}
		dir = utils_free("http.dir", dir);
	}

	/* Apply scripting only for .html requests for now - may be altered later */
	if (strstr(path, ".html") != NULL) {
		char *filename = replace(path, ".html", "");
		if (filename != NULL) {
			dir = project_info_get("dir_scripts");
			if (dir != NULL) {
				char *tmp = project_info_get("dir_root");
				char loc[4096] = { 0 };
				if (tmp != NULL) {
					snprintf(loc, sizeof(loc), "%s/%s%s.cdv", tmp, dir, filename);
					DPRINTF("%s: Script file is '%s'\n", __FUNCTION__, loc);
					script_set_descriptors(io, connected, 1);
					gHost = strdup(host);
					if (run_script(loc) != 0) {
						http_host_header(io, connected, HTTP_CODE_BAD_REQUEST, host, "text/html", NULL, myRealm, 0);
						cleanup();
						return 1;
					}

					DPRINTF("%s: Script run successfully\n", __FUNCTION__);
					method = utils_free("http.method", method);
					path = utils_free("http.path", path);
					cleanup();
					return 1;
				}
			}
		}
	}

	/* TODO: Implement the CDV-code/script processing to use IDB and other features */

	/* If still not found then return error to response */
	if (found == 0)
		http_host_page_not_found(io, connected, path);

	method = utils_free("http.method", method);
	path = utils_free("http.path", path);
	cleanup();
	return 1;
}

