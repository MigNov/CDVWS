#define DEBUG_UTILS

/* Those functions are called very often so it could be disabled */
#define DEBUG_TRIM	0
#define DEBUG_TOKENIZER	0

#include "cdvws.h"

#ifdef DEBUG_UTILS
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/utils       ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int first_initialize(int enabled)
{
	_dump_fp = NULL;
	_shell_project_loaded = 0;
	_shell_history_file = NULL;
	_shell_enabled = enabled;
	_pids = NULL;
	_pids_num = 0;
	_cdv_cookie = NULL;
	_vars = NULL;
	_vars_num = 0;
	_var_overwrite = 0;
	_perf_measure = 0;
	gIO = NULL;
	gFd = -1;

	return 0;
}

int initialize(void)
{
	int err;

	xml_init();
	project_info_init();
	if ((err = config_initialize()) != 0)
		return -err;

	if ((err = modules_initialize()) != 0)
		return -err;

	if ((err = definitions_initialize()) != 0)
		return -err;

	#ifdef USE_INTERNAL_DB
	if ((err = idb_init()) != 0)
		return -err;
	#endif

	return 0;
}

void cleanup(void)
{
	project_info_cleanup();
	definitions_cleanup();
	config_free();
	xml_cleanup();
	variable_free_all();
	regex_free();

	#ifdef USE_INTERNAL_DB
	idb_free();
	#endif
}

void total_cleanup(void)
{
	cleanup();

	if (_dump_fp != NULL) {
		fclose(_dump_fp);
		_dump_fp = NULL;
	}
}

int get_boolean(char *val)
{
	int ret = -1;
	char *val2 = NULL;

	if (val == NULL)
		return -1;

	val2 = trim(val);
	if ((strcmp(val2, "1") == 0)
		|| (strcmp(val2, "true") == 0))
		ret = 1;
	else
	if ((strcmp(val2, "0") == 0)
		|| (strcmp(val2, "false") == 0))
		ret = 0;
	free(val2);

	return ret;
}

struct timespec utils_get_time(int diff)
{
	struct timespec ts;
	struct timespec res_ts;
	struct timespec tsNull = { 0 };

	clock_gettime(CLOCK_REALTIME, &ts);
	if (diff == 1) {
		if ((_idb_last_ts.tv_nsec == 0)
			&& (_idb_last_ts.tv_sec == 0)) {
			DPRINTF("%s: No time measured yet, cannot get difference\n",
				__FUNCTION__);
			res_ts = tsNull;
		}
		else
			timespecsub(&ts, &_idb_last_ts, &res_ts);
	}
	else
		res_ts = ts;

	_idb_last_ts = ts;

	return res_ts;
}


float get_time_float_us(struct timespec ts, struct timespec te)
{
	struct timespec res_ts;
	timespecsub(&ts, &te, &res_ts);
	return ((res_ts.tv_sec * 1000000000) + res_ts.tv_nsec) / (float)1000;
}

int ensure_directory_existence(char *dir)
{
	struct stat st;

	stat(dir, &st);
	if (S_ISDIR(st.st_mode))
		return 0;
	else {
		int i;
		tTokenizer t;
		char tmp[4096] = { 0 };

		t = tokenize(dir, "/");
		for (i = 0; i < t.numTokens; i++) {
			strcat(tmp, t.tokens[i]);
			strcat(tmp, "/");

			mkdir(tmp, 0755);
		}
		mkdir(dir, 0755);

		return access(dir, X_OK);
	}
}

char *generate_hex_chars(int len)
{
	char hex[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
	char *tmp = NULL;
	int i;

	tmp = (char *)malloc( (len + 1) * sizeof(char) );
	memset(tmp, 0, (len + 1) * sizeof(char));

	for (i = 0; i < len; i++) {
		srand(time(NULL) + i);
		tmp[i] = hex[rand() % strlen(hex)];
	}

	return tmp;
}

int data_write(int fd, const void *data, size_t len, long *size)
{
	size_t sz;

	sz = write(fd, data, len);
	if (sz != len)
		return -EIO;

	if (size != NULL)
		*size += (long)sz;

	return 0;
}

unsigned char *data_fetch(int fd, int len, long *size, int extra)
{
	ssize_t sz;
	unsigned char *data = NULL;

	data = (unsigned char *) malloc( (len + extra) * sizeof(unsigned char) );
	memset(data, 0, (len + extra) * sizeof(unsigned char));
	sz = read(fd, data, len);
	if (sz != len)
		return NULL;

	if (size != NULL)
		*size += (long)sz;

	return data;
}

void project_info_init(void)
{
	project_info.root_dir = NULL;
	project_info.name = NULL;
	project_info.admin_name = NULL;
	project_info.admin_mail = NULL;
	project_info.file_config = NULL;
	project_info.dir_defs = NULL;
	project_info.dir_views = NULL;
	project_info.dir_files = NULL;
	project_info.dir_scripts = NULL;

	project_info.host_http = NULL;
	project_info.host_secure = NULL;
	project_info.cert_dir = NULL;
	project_info.cert_root = NULL;
	project_info.cert_pk = NULL;
	project_info.cert_pub = NULL;

	project_info.path_xmlrpc = NULL;
	project_info.path_rewriter = NULL;
}

void project_info_fill(void)
{
	project_info.root_dir = strdup( _proj_root_dir );
	project_info.name = strdup( config_get("project", "name") );
	project_info.admin_name = strdup( config_get("project", "administrator.name") );
	project_info.admin_mail = config_get("project", "administrator.email");
	project_info.file_config = config_get("project", "config.file");
	project_info.dir_defs = config_get("project", "config.directory.definitions");
	project_info.dir_views = config_get("project", "config.directory.views");
	project_info.dir_files = config_get("project", "config.directory.files");
	project_info.dir_scripts = config_get("project", "config.directory.scripts");

	project_info.host_http = config_get("host", "http");
	project_info.host_secure = config_get("host", "secure");
	project_info.cert_dir = config_get("host", "certificate.dir");
	project_info.cert_root = config_get("host", "certificate.root");
	project_info.cert_pk = config_get("host", "certificate.private");
	project_info.cert_pub = config_get("host", "certificate.public");

	project_info.path_xmlrpc = config_get("host", "path.xmlrpc");
	project_info.path_rewriter = config_get("host", "path.rewriter");

	DPRINTF("%s: Project information structure set\n", __FUNCTION__);
}

void project_info_dump(void)
{
	int num = 0;

	dump_printf("\nProject hosting information dump:\n");

	if (project_info.host_http != NULL) {
		dump_printf("\tHTTP host: '%s'\n", project_info.host_http);
		num++;
	}
	if (project_info.host_secure != NULL) {
		dump_printf("\tHTTPS host: '%s'\n", project_info.host_secure);
		num++;
	}
	if (project_info.cert_dir != NULL) {
		dump_printf("\tHTTPS certificate directory: '%s'\n", project_info.cert_dir);
		num++;
	}
	if (project_info.cert_root != NULL) {
		dump_printf("\tHTTPS root certificate: '%s'\n", project_info.cert_root);
		num++;
	}
	if ((project_info.cert_pk != NULL) && (project_info.cert_pub != NULL)) {
		dump_printf("\tHTTPS certificates: private '%s', public '%s'\n",
			project_info.cert_pk, project_info.cert_pub);
		num++;
	}

	if (project_info.path_xmlrpc != NULL) {
		dump_printf("\tXMLRPC Path: %s\n", project_info.path_xmlrpc);
		num++;
	}

	if (project_info.path_rewriter != NULL) {
		dump_printf("\tRewriter path: %s\n", project_info.path_rewriter);
		num++;
	}

	dump_printf("\nProject information dump:\n");

	if (project_info.name != NULL) {
		dump_printf("\tProject name: '%s'\n", project_info.name);
		num++;
	}
	if (project_info.root_dir != NULL) {
		dump_printf("\tProject root directory: '%s'\n", project_info.root_dir);
		num++;
	}
	if (project_info.admin_name != NULL) {
		dump_printf("\tProject administrator: '%s'\n", project_info.admin_name);
		num++;
	}
	if (project_info.admin_mail != NULL) {
		dump_printf("\tProject administrator's e-mail: '%s'\n", project_info.admin_mail);
		num++;
	}
	if (project_info.file_config != NULL) {
		dump_printf("\tProject config file: '%s'\n", project_info.file_config);
		num++;
	}
	if (project_info.dir_defs != NULL) {
		dump_printf("\tProject definitions dir: '%s'\n", project_info.dir_defs);
		num++;
	}
	if (project_info.dir_views != NULL) {
		dump_printf("\tProject views dir: '%s'\n", project_info.dir_views);
		num++;
	}
	if (project_info.dir_files != NULL) {
		dump_printf("\tProject files dir: '%s'\n", project_info.dir_files);
		num++;
	}
	if (project_info.dir_scripts != NULL) {
		dump_printf("\tProject script files: '%s'\n", project_info.dir_scripts);
		num++;
	}

	if (num == 0)
		dump_printf("\tNo project data available\n");

	dump_printf("\n");
}

char *project_info_get(char *type)
{
	if ((project_info.name != NULL) && (strcmp(type, "name") == 0))
		return strdup(project_info.name);
	if ((project_info.admin_name != NULL)  && (strcmp(type, "admin_name") == 0))
		return strdup(project_info.admin_name);
	if ((project_info.root_dir != NULL) && (strcmp(type, "dir_root") == 0))
		return strdup(project_info.root_dir);
	if ((project_info.admin_mail != NULL) && (strcmp(type, "admin_mail") == 0))
		return strdup(project_info.admin_mail);
	if ((project_info.file_config != NULL) && (strcmp(type, "file_config") == 0))
		return strdup(project_info.file_config);
	if ((project_info.dir_defs != NULL) && (strcmp(type, "dir_defs") == 0))
		return strdup(project_info.dir_defs);
	if ((project_info.dir_views != NULL) && (strcmp(type, "dir_views") == 0))
		return strdup(project_info.dir_views);
	if ((project_info.dir_files != NULL) && (strcmp(type, "dir_files") == 0))
		return strdup(project_info.dir_files);
	if ((project_info.dir_scripts != NULL) && (strcmp(type, "dir_scripts") == 0))
		return strdup(project_info.dir_scripts);
	if ((project_info.host_http != NULL) && (strcmp(type, "host_http") == 0))
		return strdup(project_info.host_http);
	if ((project_info.host_secure != NULL) && (strcmp(type, "host_secure") == 0))
		return strdup(project_info.host_secure);
	if ((project_info.cert_dir != NULL) && (strcmp(type, "cert_dir") == 0))
		return strdup(project_info.cert_dir);
	if ((project_info.cert_root != NULL) && (strcmp(type, "cert_root") == 0))
		return strdup(project_info.cert_root);
	if ((project_info.cert_pk != NULL) && (strcmp(type, "cert_pk") == 0))
		return strdup(project_info.cert_pk);
	if ((project_info.cert_pub != NULL) && (strcmp(type, "cert_pub") == 0))
		return strdup(project_info.cert_pub);
	if ((project_info.path_xmlrpc != NULL) && (strcmp(type, "path_xmlrpc") == 0))
		return strdup(project_info.path_xmlrpc);
	if ((project_info.path_rewriter != NULL) && (strcmp(type, "path_rewriter") == 0))
		return strdup(project_info.path_rewriter);

	return NULL;
}

void project_info_cleanup(void)
{
	free(project_info.name);
	free(project_info.root_dir);
	free(project_info.admin_name);
	free(project_info.admin_mail);
	free(project_info.file_config);
	free(project_info.dir_defs);
	free(project_info.dir_views);
	free(project_info.dir_files);
	free(project_info.dir_scripts);
	free(project_info.host_http);
	free(project_info.host_secure);
	free(project_info.cert_dir);
	free(project_info.cert_root);
	free(project_info.cert_pk);
	free(project_info.cert_pub);
	free(project_info.path_xmlrpc);
	free(project_info.path_rewriter);

	/* To set to NULLs */
	project_info_init();
}

int dump_set_file(char *filename)
{
	unlink(filename);

	_dump_fp = fopen(filename, "a");
	return (_dump_fp == NULL) ? -EINVAL : 0;
}

void dump_unset_file(void)
{
	if (_dump_fp == NULL)
		return;

	fclose(_dump_fp);
	_dump_fp = NULL;
}


char *replace(char *str, char *what, char *with)
{
	int size, idx;
	char *new, *part, *old;

	part = strstr(str, what);
	if (part == NULL)
		return str;

	size = strlen(str) - strlen(what) + strlen(with);
	new = (char *)malloc( size * sizeof(char) );
	old = strdup(str);
	idx = strlen(str) - strlen(part);
	old[idx] = 0;
	strcpy(new, old);
	strcat(new, with);
	strcat(new, part + strlen(what) );
	part = NULL;
	old = NULL;
	return new;
}

void dump_printf(const char *fmt, ...)
{
	va_list ap;
	char buf[4096] = { 0 };

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	fputs( buf, (_dump_fp != NULL) ? _dump_fp : stdout);
	va_end(ap);
}

int cdvPrintfAppend(char *var, int max_len, const char *fmt, ...)
{
	va_list ap;
	char buf[8192] = { 0 };

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);

	if ((strlen(var) + strlen(buf)) < max_len) {
		strcat(var, buf);
		return strlen(var);
	}

	return -ENOSPC;
}

char *cdvStringAppend(char *var, char *val)
{
	int size;

	if (var == NULL) {
		size = (strlen(val) + 1) * sizeof(char);
		var = (char *)malloc( size );
		memset(var, 0, size);
	}
	else {
		size = (strlen(var) + strlen(val) + 1) * sizeof(char);
		var = (char *)realloc( var, size );
	}

	if (var == NULL)
		return NULL;

	strcat(var, val);
	return var;
}

int is_numeric(char *val)
{
	int i, ok;

	ok = 1;
	for (i = 0; i < strlen(val); i++)
		if ((val[i] < '0') || (val[i] > '9'))
			ok = 0;

	return ok;
}

int is_string(char *val)
{
	if ((val == NULL) || (strlen(val) == 0))
		return 0;

	if (((val[0] == '\'') || (val[0] == '"'))
		&& ((val[strlen(val) - 1] == '\'') || (val[strlen(val) - 1] == '"')))
		return 1;

	return 0;
}

/*
 * Gets information whether it's a comment or not
 * Returns: 0 for not a comment, 1 for one line comment
 * 		2 for open comment, 3 for close comment
 */
int is_comment(char *val)
{
	int i;
	int num = 0;
	int numA = 0;

	if ((val == NULL) || (strlen(val) == 0))
		return 0;

	for (i = 0; i < strlen(val); i++) {
		if (val[i] == '/') {
			num++;

			if (num == 2)
				break;
		}
		else
		if (val[i] == '*')
			numA++;
		else
		if ((val[i] == '*') && (num == 1))
			return 2;
		else
		if ((val[i] == '/') && (numA == 1))
			return 3;
		else
			num = 0;
	}

	return (num == 2);
}

int gettype(char *val)
{
	if (is_numeric(val))
		return (atoi(val) == atol(val)) ? TYPE_INT : TYPE_LONG;

	if ((strstr(val, ".") != NULL) || (strstr(val, ",") != NULL))
		return TYPE_DOUBLE;

	return TYPE_STRING;
}

int get_type_from_string(char *type, int allow_autodetection)
{
	if ((strcmp(type, "auto") == 0) && (allow_autodetection))
		return 0;
	if (strcmp(type, "string") == 0)
		return TYPE_STRING;
	if (strcmp(type, "int") == 0)
		return TYPE_INT;
	if (strcmp(type, "long") == 0)
		return TYPE_LONG;
	if (strcmp(type, "double") == 0)
		return TYPE_DOUBLE;

	return -1;
}

char *get_type_string(int id)
{
	if (id == TYPE_INT)
		return strdup("int");
	else
	if (id == TYPE_LONG)
		return strdup("long");
	else
	if (id == TYPE_DOUBLE)
		return strdup("double");
	else
	if (id == TYPE_STRING)
		return strdup("string");
	else
		return strdup("unknown");
}

void desc_printf(BIO *io, int fd, const char *fmt, ...)
{
	va_list ap;
	char buf[8192] = { 0 };

	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	write_common(io, fd, buf, strlen(buf));
	va_end(ap);
}

char *desc_read(BIO *io, int fd)
{
	int len, tlen;
	char *ret = NULL;
	char buf[1024] = { 0 };

	tlen = 0;
	ret = (char *)malloc( sizeof(char) );

	if (io != NULL) {
		while ((len = BIO_gets(io, buf, sizeof(buf))) > 0) {
			tlen += len;
			ret = (char *)realloc(buf, (tlen + 1) * sizeof(char));

			strcat(ret, buf);
		}
	}
	else {
		while ((len = read(fd, buf, sizeof(buf))) > 0) {
			tlen += len;
			ret = (char *)realloc(buf, (tlen + 1) * sizeof(char));

			strcat(ret, buf);
		}
	}

	return ret;
}

int dump_file_is_set(void)
{
	return (_dump_fp == NULL) ? 0 : 1;
}

long get_file_size(char *filename)
{
	long size;
	struct stat st;

	stat(filename, &st);
	size = st.st_size;

	return size;
}

char *get_full_path(char *basedir)
{
        char buf[BUFSIZE] = { 0 };
        char buf2[BUFSIZE] = { 0 };

	if ((basedir == NULL) || (strlen(basedir) == 0))
		return NULL;

	if ((basedir != NULL) && (strlen(basedir) > 1)) {
		if ((basedir[0] == '.') && (basedir[1] == '/')) {
			getcwd(buf2, sizeof(buf2));

			snprintf(buf, sizeof(buf), "%s/%s", buf2, basedir + 2);
		}
		else
			snprintf(buf, sizeof(buf), "%s", basedir);
	}
	else
		snprintf(buf, sizeof(buf), "%s", basedir);

	DPRINTF("%s: Base directory set to '%s'\n", __FUNCTION__, basedir);
	return strdup(buf);
}

int load_project(char *project_file)
{
	int ret = 0;
	int err = 0;
	char *tmp = NULL;
	char path[BUFSIZE] = { 0 };
	char *project_basedir = NULL;

	project_basedir = get_full_path( dirname( strdup(project_file) ) );
	basedir = strdup(project_basedir);

	_proj_root_dir = basedir;

	if (access(project_file, R_OK) != 0) {
		ret = -ENOENT;
		goto finish;
	}

	if (initialize() != 0) {
		ret = -EIO;
		goto finish;
	}

	if ((err = config_load(project_file, NULL)) != 0) {
		ret = -EINVAL;
		goto finish;
	}

	project_info_fill();

	/* Reinit as we don't want to expose project variables to the configVars */
	config_free();

	/* Preserve basedir */
	basedir = strdup(project_basedir);
	config_initialize();

	tmp = project_info_get("file_config");
	if (tmp == NULL) {
		ret = -EINVAL;
		goto finish;
	}

	snprintf(path, sizeof(path), "%s/%s", project_basedir, tmp);
	free(tmp);
	tmp = NULL;

	if ((err = config_load(path, NULL)) != 0) {
		ret = -EINVAL;
		goto finish;
	}

	DPRINTF("%s: All configurations loaded\n", __FUNCTION__);

	tmp = project_info_get("dir_defs");
	if (tmp == NULL) {
		ret = -EINVAL;
		goto finish;
	}

	snprintf(path, sizeof(path), "%s/%s", project_basedir, tmp);
	free(tmp);
	tmp = NULL;

	DPRINTF("%s: Definitions directory is '%s'\n", __FUNCTION__, path);
	if (definitions_load_directory(path) != 0) {
		ret = -EIO;
		goto finish;
	}

finish:
	if (ret != 0) {
		DPRINTF("%s: Invalid error code %d, cleaning up\n", __FUNCTION__,
			ret);
		cleanup();
	}

	return ret;
}

void project_dump(void)
{
	project_info_dump();
	config_variable_dump(NULL);
}

int _try_project_match(char *pd, char *host)
{
	int ret;

	struct dirent *de = NULL;
	DIR *d = opendir(pd);

	if (host == NULL) {
		DPRINTF("%s: Host to match cannot be null\n", __FUNCTION__);
		return -EINVAL;
	}

	if (d == NULL)
		ret = -EIO;
	else {
		ret = -ENOENT;
		while ((de = readdir(d)) != NULL) {
			if (strstr(de->d_name, ".project") != NULL) {
				char tmp[4096] = { 0 };

				snprintf(tmp, sizeof(tmp), "%s/%s",
					pd, de->d_name);

				if (load_project(tmp) == 0) {
					char *host_http = project_info_get("host_http");
					char *host_secure = project_info_get("host_secure");

					if (
						((host_http != NULL) && (strcmp(host_http, host) == 0))
						|| ((host_secure != NULL) && (strcmp(host_secure, host) == 0))
						) {
								project_dump();
								ret = 0;
								break;
					}
					else {
						DPRINTF("%s: Project doesn't match ((%s != %s) && (%s != %s))\n",
							__FUNCTION__, host, host_http ? host_http : "<null>",
								host, host_secure ? host_secure : "<null>");
					}

					DPRINTF("%s: Cleaning up\n", __FUNCTION__);
					cleanup();
				}
			}
		}
	}
	closedir(d);

	return ret;
}

int find_project_for_web(char *directory, char *host)
{
	int ret;
	struct dirent *de = NULL;
	DIR *d = opendir(directory);
	if (d == NULL)
		ret = -EBADF;
	else {
		ret = -ENOENT;
		while ((de = readdir(d)) != NULL) {
			char tmp[4096] = { 0 };

			if (de->d_name[0] != '.') {
				snprintf(tmp, sizeof(tmp), "%s/%s", directory,
					de->d_name);

				if (_try_project_match(tmp, host) == 0) {
					ret = 0;
					break;
				}
			}
		}
	}
	closedir(d);

	return ret;
}

int load_project_directory(char *project_path)
{
	int ret = 0;
	int err = 0;
	char path[BUFSIZE] = { 0 };

	if (initialize() != 0) {
		ret = -EIO;
		goto finish;
	}

	snprintf(path, sizeof(path), "%s/config", project_path);
	if ((err = config_load(path, NULL)) != 0) {
		ret = -EINVAL;
		goto finish;
	}

	/* TODO: Implement definition processing and view substitutions */

finish:
	cleanup();

	return ret;
}

tTokenizer tokenize(char *string, char *by)
{
        char *tmp;
        char *str;
        char *save;
        char *token;
        int i = 0;
        tTokenizer t;

	if (DEBUG_TOKENIZER)
		DPRINTF("%s: Tokenizing string '%s' by '%s'\n", __FUNCTION__, string, by);

        tmp = strdup(string);
        t.tokens = malloc( sizeof(char *) );
        for (str = tmp; ; str = NULL) {
                token = strtok_r(str, by, &save);
                if (token == NULL)
                        break;

                t.tokens = realloc( t.tokens, (i + 1) * sizeof(char *) );
                t.tokens[i++] = strdup(token);
        }

        t.numTokens = i;

	if (DEBUG_TOKENIZER)
		DPRINTF("%s: Tokenized to %d tokens\n", __FUNCTION__, i);

        return t;
}

void free_tokens(tTokenizer t)
{
        int i;

	if (DEBUG_TOKENIZER)
		DPRINTF("%s: Freeing all tokens\n", __FUNCTION__);

        for (i = 0; i < t.numTokens; i++) {
                free(t.tokens[i]);
                t.tokens[i] = NULL;
        }
}

char *trim(char *str)
{
        int i;

	/* Null remains still null ;-) */
	if (str == NULL)
		return NULL;

	if (DEBUG_TRIM)
		DPRINTF("%s: Trimming string '%s'\n", __FUNCTION__, str);

        if (strchr(str, ' ') != NULL)
                while (*str == ' ')
                        *str++;

	if (strchr(str, '\t') != NULL)
		while (*str == '\t')
			*str++;

        for (i = strlen(str); i > 0; i--)
                if (str[i] != ' ')
                        break;

        i--;
        if ((strlen(str) >= i) && ((str[i] == '\n') || (str[i] == ' ')))
                str[i] = 0;

	if (DEBUG_TRIM)
		DPRINTF("%s: Resulting string is '%s'\n", __FUNCTION__, str);

        return strdup(str);
}

char *get_mime_type_match(char *path)
{
	int i;
	FILE *fp = NULL;
	char tmp[4096] = { 0 };
	char *ret = NULL;
	char *ext = NULL;

	fp = fopen("/etc/mime.types", "r");
	if (fp == NULL)
		return NULL;

	ext = strstr(path, ".");
	if (ext == NULL)
		return NULL;

	ext++;
	while (!feof(fp)) {
		memset(tmp, 0, sizeof(tmp));
		fgets(tmp, sizeof(tmp), fp);

		if (strlen(tmp) > 0) {
			if (tmp[strlen(tmp) - 1] == '\n')
				tmp[strlen(tmp) - 1] = 0;

			tTokenizer t = tokenize(tmp, "\t");
			for (i = 0; i < t.numTokens; i++) {
				if (strcmp(t.tokens[i], ext) == 0) {
					ret = strdup( t.tokens[0] );
					break;
				}
			}
			free_tokens(t);
		}
	}

	fclose(fp);
	return ret;
}

char *get_mime_type_cmd(char *path)
{
	FILE *fp = NULL;
	char tmp[4096] = { 0 };

	if (access(path, R_OK) != 0)
		return NULL;

	snprintf(tmp, sizeof(tmp), "file --mime-type -b %s", path);
	fp = popen(tmp, "r");
	memset(tmp, 0, sizeof(tmp));
	fgets(tmp, sizeof(tmp), fp);
	fclose(fp);

	if (tmp[strlen(tmp) - 1] == '\n')
		tmp[strlen(tmp) - 1] = 0;

	return strdup( tmp );
}

char *get_mime_type(char *path)
{
	char *ret = NULL;

	ret = get_mime_type_match(path);
	if (ret == NULL)
		ret = get_mime_type_cmd(path);

	return ret;
}

char *process_read_handler(char *filename)
{
        FILE *fp = NULL;
        char data[4096] = { 0 };

        if ((filename == NULL) || (strlen(filename) == 0))
                return NULL;

	DPRINTF("%s: Processing read handler for '%s'\n", __FUNCTION__, filename);

        if (filename[0] != '/') {
                char buf[4096] = { 0 };

                getcwd(buf, sizeof(buf));
                if ((strlen(buf) + strlen(filename) + 1) < sizeof(buf)) {
                        strcat(buf, "/");
                        strcat(buf, filename);
                }

                filename = strdup(buf);
        }

        if (access(filename, R_OK) != 0)
                return NULL;

        fp = fopen(filename, "r");
        if (fp == NULL)
                return NULL;

        fgets(data, sizeof(data), fp);
        fclose(fp);

        free(filename);

        return strdup(data);
}

char *process_exec_handler(char *binary)
{
        FILE *fp = NULL;
        char s[4096] = { 0 };

        if (access(binary, X_OK) != 0)
                return NULL;

	DPRINTF("%s: Processing exec handler for '%s'\n", __FUNCTION__, binary);

        fp = popen(binary, "r");
        if (fp == NULL)
                return NULL;

        fgets(s, 4096, fp);
        fclose(fp);

        /* Strip \n from the end */
        if (s[strlen(s) - 1] == '\n')
                s[strlen(s) - 1] = 0;

        return strdup(s);
}

char *process_handlers(char *path)
{
        if (path == NULL)
                return NULL;

	DPRINTF("%s: Processing '%s' using handlers\n", __FUNCTION__, path);

        if (strncmp(path, "read://", 7) == 0)
                return process_read_handler(path + 7);
        if (strncmp(path, "exec://", 7) == 0)
                return process_exec_handler(path + 7);

	DPRINTF("%s: No handler found for '%s'\n", __FUNCTION__, path);
        return NULL;
}

/* PID functions */
void utils_pid_add(int pid)
{
	if (_pids == NULL) {
		_pids = (int *)malloc( sizeof(int) );
		_pids_num = 0;
	}
	else
		_pids = (int *)realloc( _pids, (_pids_num + 1) * sizeof(int) );

	_pids[ _pids_num ] = pid;
	_pids_num++;
}

void utils_pid_dump(void)
{
	int i;

	if (_pids == NULL)
		return;

	for (i = 0; i < _pids_num; i++)
		dump_printf("PID #%d: %d\n", i + 1, _pids[i]);
}

int utils_pid_signal_all(int sig)
{
	int i;

	if (_pids == NULL)
		return 0;

	for (i = 0; i < _pids_num; i++) {
		DPRINTF("%s: Sending signal %d to PID %d\n",
			__FUNCTION__, sig, _pids[i]);
		kill(_pids[i], sig);
	}

	return _pids_num;

}

int utils_pid_wait_all(void)
{
	int i, new, old;

	if (_pids == NULL)
		return 0;

	new = _pids_num;
	old = new;
	for (i = 0; i < new; i++) {
		waitpid( _pids[i], NULL, 0 );
		new--;
	}
	_pids_num = 0;

	return old;
}

int utils_pid_kill_all(void)
{
	return utils_pid_signal_all(SIGKILL);
}

