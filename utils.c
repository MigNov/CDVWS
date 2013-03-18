#define DEBUG_UTILS

/* Debug memory allocator routines (incl. free()) */
//#define DEBUG_MEMORY
//#define DEBUG_MEMORY_NULL

/* Those functions are called very often so it could be disabled */
#define DEBUG_TRIM	0
#define DEBUG_TOKENIZER	0

#include "cdvws.h"

#ifdef DEBUG_UTILS
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/utils       ] " fmt , args); } while (0)
	#ifdef DEBUG_MEMORY
		#define DPRINTF_MEM(fmt, args...) \
		do { fprintf(stderr, "[cdv/utils-mem   ] " fmt , args); } while (0)
		#ifdef DEBUG_MEMORY_NULL
			#define DPRINTF_MEM_NULL(fmt, args...) \
			do { fprintf(stderr, "[cdv/utils-mem-0 ] " fmt , args); } while (0)
		#else
			#define DPRINTF_MEM_NULL(fmt, args...) do {} while(0)
		#endif
	#else
		#define DPRINTF_MEM(fmt, args...) do {} while(0)
		#define DPRINTF_MEM_NULL(fmt, args...) do {} while(0)
	#endif
#else
#define DPRINTF(fmt, args...) do {} while(0)
#define DPRINTF_MEM(fmt, args...) do {} while(0)
#define DPRINTF_MEM_NULL(fmt, args...) do {} while(0)
#endif

int first_initialize(int enabled)
{
	_tcp_sock_fds_num = 0;
	_dump_fp = NULL;
	_shell_project_loaded = 0;
	_shell_history_file = NULL;
	_shell_enabled = enabled;
	//_pids = NULL;
	//_pids_num = 0;
	shared_mem = NULL;
	_cdv_cookie = NULL;
	_vars = NULL;
	_vars_num = 0;
	_var_overwrite = 0;
	_perf_measure = 0;
	_script_in_condition_and_met = -1;
	_handlers_path = NULL;
	gIO = NULL;
	gFd = -1;
	gHttpHandler = 0;
	myRealm = NULL;

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
	val2 = utils_free("utils.get_boolean.val2", val2);

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

	tmp = (char *)utils_alloc( "utils.generate_hex_chars", (len + 1) * sizeof(char) );
	memset(tmp, 0, (len + 1) * sizeof(char));

	for (i = 0; i < len; i++) {
		srand(time(NULL) + i);
		tmp[i] = hex[rand() % strlen(hex)];
	}

	return tmp;
}

char *generate_hash(char *str, char *salt, int len)
{
	int olen;
	int i, j, c, k, l;
	char *ret = NULL;
	char tmp[1024] = { 0 };

	if (len % 2 != 0) {
		DPRINTF("%s: Invalid length. Length have to be multiple of 2\n", __FUNCTION__);
		return NULL;
	}

	olen = len;

	/* Divide length by two first */
	len /= 2;

	ret = (char *)utils_alloc( "utils.generate_hash.ret", (olen + 1) * sizeof(char) );
	memset(ret, 0, ((2 * len) + 1) * sizeof(char));

	/* First of all we generate the initial salt value */
	j = 0;
	for (i = 0; i < strlen(salt); i++)
		j += salt[i];

	DPRINTF("%s: Computed initial salt value to 0x%x\n", __FUNCTION__, j);

	/* Then we generate the initial string value */
	k = 0;
	for (i = 0; i < strlen(str); i++)
		k += str[i];

	l = j;
	j *= k;
	k /= l;

	j += olen - l;
	k *= olen;

	DPRINTF("%s: Computed initial string value to 0x%x\n", __FUNCTION__, k);

	/* And we iterate all the string and do hash calculations */
	for (i = 0; i < strlen(str); i++) {
		c = (str[(i + k) % strlen(str)] * salt[(i + j) % strlen(salt)] + j) % 65536;
		DPRINTF("%s: c#%03d: %06d, j = %10d, k = %5d, c(h) = 0x%04x\n", __FUNCTION__, i, c, j, k, c);

		snprintf(tmp, sizeof(tmp), "%04x", c);
		strcat(ret, tmp);

		l = str[i] & salt[i % strlen(salt)];
		switch (l % 16) {
			case 0: k += l; j += l;
				break;
			case 1: k += l; j -= l;
				break;
			case 2: k += l; j *= l;
				break;
			case 3: k += l; j /= l;
				break;
			case 4: k -= l; j += l;
				break;
			case 5: k -= l; j -= l;
				break;
			case 6: k -= l; j *= l;
				break;
			case 7: k -= l; j /= l;
				break;
			case 8: k *= l; j += l;
				break;
			case 9: k *= l; j -= l;
				break;
			case 10: k *= l; j *= l;
				break;
			case 11: k *= l; j /= l;
				break;
			case 12: k /= l; j += l;
				break;
			case 13: k /= l; j -= l;
				break;
			case 14: k /= l; j *= l;
				break;
			case 15: k /= l; j /= l;
				break;
		}
	}

	DPRINTF("%s: Total entropy length in hexadecimal format is %d bytes\n",
		__FUNCTION__, (int)strlen(ret));

	/* Calculate rest to the required hash length */
	j += strlen(ret);
	while (strlen(ret) < olen) {
		j += k * l;

		snprintf(tmp, sizeof(tmp), "%04x", ((j + k - l) * (int)strlen(ret)) % 65536 );
		if (((2 * len) + 1) - strlen(ret) < 4)
			tmp[ ((2 * len) + 1) - (int)strlen(ret) - 1 ] = 0;
		strcat(ret, tmp);
	}

	ret[olen] = 0;

	DPRINTF("%s: Total hash length in hexadecimal format is %d bytes\n",
		__FUNCTION__, (int)strlen(ret));

	DPRINTF("%s('%s', '%s', %d) returning %d bytes\n", __FUNCTION__, str, salt, olen,
		(int)strlen(ret));
	DPRINTF("    returning '%s'\n", ret);

	return ret;
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

	data = (unsigned char *) utils_alloc( "utils.data_fetch", (len + extra) * sizeof(unsigned char) );
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

	project_info.kerb_keytab = NULL;
	project_info.kerb_secure_path = NULL;
	project_info.kerb_realm = NULL;
	project_info.kerb_realm_fb = NULL;

	project_info.geoip_enable = NULL;
	project_info.geoip_expose = NULL;
	project_info.geoip_file = NULL;

	project_info.idbadmin_enable = NULL;
	project_info.idbadmin_table = NULL;
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

	project_info.kerb_keytab = config_get("kerberos", "keytab");
	project_info.kerb_secure_path = config_get("kerberos", "secure_path");
	project_info.kerb_realm = config_get("kerberos", "realm");
	project_info.kerb_realm_fb = config_get("kerberos", "realm_basic");

	project_info.geoip_enable = config_get("geoip", "enable");
	project_info.geoip_expose = config_get("geoip", "expose");
	project_info.geoip_file = config_get("geoip", "file");

	project_info.idbadmin_enable = config_get("idbadmin", "enable");
	project_info.idbadmin_table = config_get("idbadmin", "admintbl");

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

	if (project_info.kerb_keytab != NULL) {
		dump_printf("\tKerberos keytab: %s\n", project_info.kerb_keytab);
		num++;
	}

	if (project_info.kerb_secure_path != NULL) {
		dump_printf("\tKerberos secure path: %s\n", project_info.kerb_secure_path);
		num++;
	}

	if (project_info.kerb_realm != NULL) {
		dump_printf("\tKerberos realm: %s\n", project_info.kerb_realm);
		num++;
	}

	if (project_info.kerb_realm_fb != NULL) {
		dump_printf("\tKerberos realm for Basic authorization: %s\n", project_info.kerb_realm_fb);
		num++;
	}

	if (project_info.geoip_enable != NULL) {
		dump_printf("\tGeoIP Enabled: %s\n", project_info.geoip_enable);
		num++;
	}

	if (project_info.geoip_file != NULL) {
		dump_printf("\tGeoIP File: %s\n", project_info.geoip_file);
		num++;
	}

	if (project_info.geoip_expose != NULL) {
		dump_printf("\tGeoIP Expose Variables: %s\n", project_info.geoip_expose);
		num++;
	}

	if (project_info.idbadmin_enable != NULL) {
		dump_printf("\tiDB Admin: %s\n", project_info.idbadmin_enable);
		num++;
	}

	if (project_info.idbadmin_table != NULL) {
		dump_printf("\tiDB Admin Table: %s\n", project_info.idbadmin_table);
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
	if ((project_info.kerb_keytab != NULL) && (strcmp(type, "kerberos_keytab") == 0))
		return strdup(project_info.kerb_keytab);
	if ((project_info.kerb_secure_path != NULL) && (strcmp(type, "kerberos_secure_path") == 0))
		return strdup(project_info.kerb_secure_path);
	if ((project_info.kerb_realm) && (strcmp(type, "kerberos_realm") == 0))
		return strdup(project_info.kerb_realm);
	if ((project_info.kerb_realm_fb) && (strcmp(type, "kerberos_realm_fallback") == 0))
		return strdup(project_info.kerb_realm_fb);
	if ((project_info.geoip_enable) && (strcmp(type, "geoip_enable") == 0))
		return strdup(project_info.geoip_enable);
	if ((project_info.geoip_file) && (strcmp(type, "geoip_file") == 0))
		return strdup(project_info.geoip_file);
	if ((project_info.geoip_expose) && (strcmp(type, "geoip_expose") == 0))
		return strdup(project_info.geoip_expose);
	if ((project_info.idbadmin_enable) && (strcmp(type, "idbadmin_enable") == 0))
		return strdup(project_info.idbadmin_enable);
	if ((project_info.idbadmin_table) && (strcmp(type, "idbadmin_table") == 0))
		return strdup(project_info.idbadmin_table);
	if ((project_info.path_rewriter != NULL) && (strcmp(type, "path_rewriter") == 0))
		return strdup(project_info.path_rewriter);

	return NULL;
}

void project_info_cleanup(void)
{
	project_info.name = utils_free("utils.project_info_cleanup", project_info.name);
	project_info.root_dir = utils_free("utils.project_info_cleanup", project_info.root_dir);
	project_info.admin_name = utils_free("utils.project_info_cleanup", project_info.admin_name);
	project_info.admin_mail = utils_free("utils.project_info_cleanup", project_info.admin_mail);
	project_info.file_config = utils_free("utils.project_info_cleanup", project_info.file_config);
	project_info.dir_defs = utils_free("utils.project_info_cleanup", project_info.dir_defs);
	project_info.dir_views = utils_free("utils.project_info_cleanup", project_info.dir_views);
	project_info.dir_files = utils_free("utils.project_info_cleanup", project_info.dir_files);
	project_info.dir_scripts = utils_free("utils.project_info_cleanup", project_info.dir_scripts);
	project_info.host_http = utils_free("utils.project_info_cleanup", project_info.host_http);
	project_info.host_secure = utils_free("utils.project_info_cleanup", project_info.host_secure);
	project_info.cert_dir = utils_free("utils.project_info_cleanup", project_info.cert_dir);
	project_info.cert_root = utils_free("utils.project_info_cleanup", project_info.cert_root);
	project_info.cert_pk = utils_free("utils.project_info_cleanup", project_info.cert_pk);
	project_info.cert_pub = utils_free("utils.project_info_cleanup", project_info.cert_pub);
	project_info.path_xmlrpc = utils_free("utils.project_info_cleanup", project_info.path_xmlrpc);
	project_info.path_rewriter = utils_free("utils.project_info_cleanup", project_info.path_rewriter);
	project_info.kerb_keytab = utils_free("utils.project_info_cleanup", project_info.kerb_keytab);
	project_info.kerb_secure_path = utils_free("utils.project_info_cleanup", project_info.kerb_secure_path);
	project_info.kerb_realm = utils_free("utils.project_info_cleanup", project_info.kerb_realm);
	project_info.kerb_realm_fb = utils_free("utils.project_info_cleanup", project_info.kerb_realm_fb);
	project_info.geoip_enable = utils_free("utils.project_info_cleanup", project_info.geoip_enable);
	project_info.geoip_file = utils_free("utils.project_info_cleanup", project_info.geoip_file);
	project_info.geoip_expose = utils_free("utils.project_info_cleanup", project_info.geoip_expose);
	project_info.idbadmin_enable = utils_free("utils.project_info_cleanup", project_info.idbadmin_enable);
	project_info.idbadmin_table = utils_free("utils.project_info_cleanup", project_info.idbadmin_table);

	/* To set to NULLs */
	project_info_init();
}

void *utils_alloc(char *var, int len)
{
	void *val = NULL;

	if (len <= 0) {
		DPRINTF_MEM("%s: Invalid length argument (%d)\n", __FUNCTION__, len);
		return NULL;
	}

	val = malloc( len );
	if (val == NULL) {
		DPRINTF_MEM("%s: Allocation of %d bytes failed\n", __FUNCTION__, len);
		return NULL;
	}

	DPRINTF_MEM("%s: Setting memory to zeros\n", __FUNCTION__);
	memset(val, 0, len);

	DPRINTF_MEM("%s: Returning pointer %p (%d bytes, variable: %s)\n", __FUNCTION__, val, len, var);
	return val;
}

void *utils_free(char *vt, void *var)
{
	if (var == NULL) {
		DPRINTF_MEM_NULL("%s: Variable %s is NULL. Skipping ...\n", __FUNCTION__, vt);

		return NULL;
	}

	DPRINTF_MEM("%s: Freeing variable %s at %p\n", __FUNCTION__, vt, var);

	free(var);

	DPRINTF_MEM("%s: Setting to variable %s pointer from %p to NULL\n", __FUNCTION__, vt, var);

	return NULL;
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
	new = (char *)utils_alloc( "utils.replace", size * sizeof(char) );
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
		var = (char *)utils_alloc( "utils.cdvStringAppend", size );
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

int is_double(char *val)
{
	int i, ok;

	ok = 1;
	for (i = 0; i < strlen(val); i++)
		 if (((val[i] < '0') || (val[i] > '9')) && (val[i] != '.'))
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

	if (is_double(val))
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
	va_end(ap);

	if (gHttpHandler == 1) {
		char *tmp = strdup(buf);

		while (strstr(tmp, "\n") != NULL)
			tmp = replace(tmp, "\n", "<br />");
		while (strstr(tmp, "  ") != NULL)
			tmp = replace(tmp, "  ", "&nbsp; ");
		if (strstr(tmp, "PERF:") != NULL)
			tmp = replace(tmp, "PERF:", "<i><font color=\"red\">PERF: ");

		write_common(io, fd, tmp, strlen(tmp));

		if (strstr(tmp, "PERF:") != NULL)
			write_common(io, fd, "</font></i>", 11);
		//utils_free(tmp);
	}
	else
		write_common(io, fd, buf, strlen(buf));
}

char *desc_read(BIO *io, int fd)
{
	int len, tlen;
	char *ret = NULL;
	char buf[1024] = { 0 };

	tlen = 0;
	ret = (char *)utils_alloc( "utils.desc_read", sizeof(char) );

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
	tmp = utils_free("utils.load_project.tmp", tmp);
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
	tmp = utils_free("utils.load_project.tmp2", tmp);
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
        t.tokens = utils_alloc( "utils.tokenize", sizeof(char *) );
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

        for (i = 0; i < t.numTokens; i++)
                t.tokens[i] = utils_free("utils.free_tokens", t.tokens[i]);
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
	char loc[4096] = { 0 };

        if ((filename == NULL) || (strlen(filename) == 0))
                return NULL;

	DPRINTF("%s: Processing read handler for '%s'\n", __FUNCTION__, filename);

        if (filename[0] != '/') {
		if (_handlers_path != NULL)
			snprintf(loc, sizeof(loc), "%s/%s", _handlers_path, filename);
		else {
			char buf[4096] = { 0 };

	                getcwd(buf, sizeof(buf));
        	        if ((strlen(buf) + strlen(filename) + 1) < sizeof(buf)) {
                	        strcat(buf, "/");
                        	strcat(buf, filename);
	                }

			strcpy(loc, buf);
		}
        }
	else
		strcpy(loc, filename);

        if (access(loc, R_OK) != 0) {
		DPRINTF("%s: File %s does not exist\n", __FUNCTION__, loc);
                return NULL;
	}

        fp = fopen(loc, "r");
        if (fp == NULL)
                return NULL;

        fgets(data, sizeof(data), fp);
        fclose(fp);

	if ((strlen(data) > 0) && (data[strlen(data) - 1] == '\n'))
		data[strlen(data) - 1] = 0;

        return strdup(data);
}

char *process_exec_handler(char *binary)
{
        FILE *fp = NULL;
        char s[4096] = { 0 };
	char bin[4096] = { 0 };

	if (binary == NULL)
		return NULL;

	if (_handlers_path != NULL)
		snprintf(bin, sizeof(bin), "%s/%s", _handlers_path, binary);
	else
		strcpy(bin, binary);

        if (access(bin, X_OK) != 0)
                return NULL;

	DPRINTF("%s: Processing exec handler for '%s'\n", __FUNCTION__, bin);

        fp = popen(bin, "r");
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

void handlers_set_path(char *path)
{
	_handlers_path = strdup(path);

	DPRINTF("%s: Handlers path set to %s\n", __FUNCTION__, _handlers_path);
}

char *process_decoding(char *in, char *type)
{
	size_t len = -1;

	if (strcmp(type, "base64") == 0) {
		char *ret = NULL;

		DPRINTF("%s: Decoding string from base64\n", __FUNCTION__);

		len = strlen(in);
		ret = base64_decode(in, &len);

		if ((ret != NULL) && (strlen(ret) > 0) && (ret[strlen(ret) - 1] == '\n'))
			ret[strlen(ret) - 1] = 0;

		return ret;
	}

	return NULL;
}

int valcmp(char *a1, char *a2)
{
	if ((a1 == NULL) || (a2 == NULL))
		return 0;

	if ((strlen(a1) > 0) && ((a1[0] == '\'') || (a1[0] == '"'))) {
		*a1++;

		a1[strlen(a1) - 1] = 0;
	}
	else {
		char *tmp = variable_get_element_as_string(trim(a1), NULL);
		if (tmp)
			a1 = tmp;
		else
			a1 = trim(a1);
	}

	if ((strlen(a2) > 0) && ((a2[0] == '\'') || (a2[0] == '"'))) {
		*a2++;

		a2[strlen(a2) - 1] = 0;
	}
	else {
		char *tmp = variable_get_element_as_string(trim(a2), NULL);

		if (tmp)
			a2 = tmp;
		else
			a2 = trim(a2);
	}

	DPRINTF("%s: string1 = '%s', string2 = '%s'\n", __FUNCTION__, a1, a2);
	return strcmp(a1, a2);
}

char *get_ip_address(char *ip, int *outType)
{
	int type = TCP_IPV4;

	if (strncmp(ip, "::ffff:", 7) == 0) {
		int i;

		/* This is IPv6 mapped IPv4 address */
		for (i = 0; i < 7; i++)
			*ip++;

		type = TCP_IPV4;
	}
	else
	if (strchr(ip, ':') != NULL)
		type = TCP_IPV6;

	if (outType != NULL)
		*outType = type;

	return ip;
}

#ifdef USE_GEOIP
tGeoIPInfo geoip_get_info(char *geoip_file, char *ip)
{
	GeoIP		*gi = NULL;
	GeoIPRecord	*gir = NULL;
	uint32_t	ipnum;
	const char	*country_code;
	int		country_id, i;
	tGeoIPInfo	GeoIPInfo;
	int		ipv6 = 0;

	ip = get_ip_address(ip, NULL);

	DPRINTF("%s: Querying database for IP address %s information\n", __FUNCTION__, ip);

	/* No data acquired */
	GeoIPInfo.type = -1;

	#ifdef GEOIP_LOCAL_OVERRIDE
	if (strcmp(ip, "127.0.0.1") == 0) {
		unsigned char tmp[4];
		char tmp2[16];

		UINT32STR(tmp, GEOIP_LOCAL_OVERRIDE);
		snprintf(tmp2, sizeof(tmp2), "%d.%d.%d.%d", tmp[0], tmp[1], tmp[2], tmp[3]);
		ip = strdup(tmp2);

		DPRINTF("%s: Overridding local IP to test IP %s\n", __FUNCTION__, ip);
	}
	#endif

	memset(GeoIPInfo.addr, 0, sizeof(GeoIPInfo.addr));
	memcpy(GeoIPInfo.addr, ip, sizeof(GeoIPInfo.addr));

	memset(GeoIPInfo.country_code, 0, sizeof(GeoIPInfo.country_code));
	memset(GeoIPInfo.city, 0, sizeof(GeoIPInfo.city));
	memset(GeoIPInfo.region, 0, sizeof(GeoIPInfo.region));
	memset(GeoIPInfo.postal_code, 0, sizeof(GeoIPInfo.postal_code));

	strcpy(GeoIPInfo.country_code, "Unknown");
	strcpy(GeoIPInfo.city, "Unknown");
	strcpy(GeoIPInfo.region, "Unknown");
	strcpy(GeoIPInfo.postal_code, "Unknown");

	/* If we have colon in the IP address string it's most likely IPv6 address */
	if (strchr((char *)ip, ':') != NULL) {
		ipv6 = 1;
		DPRINTF("%s: Address %s seems to be IPv6 address\n", __FUNCTION__, ip);
	}

	gi = GeoIP_open(geoip_file, GEOIP_STANDARD);
	if (gi == NULL)
		return GeoIPInfo;

	i = GeoIP_database_edition(gi);
	if (!ipv6) {
		ipnum = _GeoIP_lookupaddress(ip);
		if (ipnum == 0)
			return GeoIPInfo;
	}
	else {
		if (__GEOIP_V6_IS_NULL(_GeoIP_lookupaddress_v6(ip)))
			return GeoIPInfo;
	}

	GeoIPInfo.ipv6 = ipv6;

	if ((i == GEOIP_CITY_EDITION_REV0) || (i == GEOIP_CITY_EDITION_REV1)
		|| (i == GEOIP_CITY_EDITION_REV0_V6) || (i == GEOIP_CITY_EDITION_REV1_V6)) {
		if (ipv6) {
			DPRINTF("%s: Querying GeoIP/GeoLite City Edition for IPv6\n", __FUNCTION__);
			if ((i == GEOIP_CITY_EDITION_REV0_V6) || (i == GEOIP_CITY_EDITION_REV1_V6))
				gir = GeoIP_record_by_name_v6(gi, ip);
		}
		else {
			DPRINTF("%s: Querying GeoIP/GeoLite City Edition for IPv4\n", __FUNCTION__);
			if ((i == GEOIP_CITY_EDITION_REV0) || (i == GEOIP_CITY_EDITION_REV1))
				gir = GeoIP_record_by_ipnum(gi, ipnum);
		}

		if (gir != NULL) {
			GeoIPInfo.type = GEOIP_CITY_EDITION_REV1;

			strncpy(GeoIPInfo.country_code, gir->country_code ? gir->country_code : "-", sizeof(GeoIPInfo.country_code));
			strncpy(GeoIPInfo.region, gir->region ? gir->region : "-", sizeof(GeoIPInfo.region));
			strncpy(GeoIPInfo.city, gir->city ? gir->city : "-", sizeof(GeoIPInfo.city));
			strncpy(GeoIPInfo.postal_code, gir->postal_code ? gir->postal_code : "-", sizeof(GeoIPInfo.postal_code));
			GeoIPInfo.geo_lat = gir->latitude;
			GeoIPInfo.geo_long = gir->longitude;
			GeoIPRecord_delete(gir);
		}
	}
	else
	if ((i == GEOIP_COUNTRY_EDITION) || (i == GEOIP_COUNTRY_EDITION_V6)) {
		if (i == GEOIP_COUNTRY_EDITION_V6) {
			const char *tmp = GeoIP_country_code_by_name_v6(gi, ip);

			DPRINTF("%s: Querying GeoIP/GeoLite Country Edition for IPv6\n", __FUNCTION__);
			if (tmp != NULL) {
				GeoIPInfo.type = GEOIP_COUNTRY_EDITION;

				strncpy(GeoIPInfo.country_code, tmp, sizeof(GeoIPInfo.country_code));
			}
		}
		else {
			country_id = GeoIP_id_by_ipnum(gi, ipnum);
			country_code = GeoIP_country_code[country_id];

			DPRINTF("%s: Querying GeoIP/GeoLite Country Edition for IPv4\n", __FUNCTION__);
			if (country_id > 0) {
				GeoIPInfo.type = GEOIP_COUNTRY_EDITION;

				strncpy(GeoIPInfo.country_code, country_code, sizeof(GeoIPInfo.country_code));
			}
		}
	}

	GeoIP_delete(gi);
	GeoIP_cleanup();

	return GeoIPInfo;
}

void geoip_info_dump(tGeoIPInfo geoip_info)
{
	if (geoip_info.type == -1) {
		if ((strcmp(geoip_info.addr, "127.0.0.1") == 0)
			|| (strcmp(geoip_info.addr, "::1") == 0))
			DPRINTF("%s: Address 127.0.0.1 is local address. No information available.\n",
				__FUNCTION__);
		else
			DPRINTF("%s: Address %s not found\n", __FUNCTION__, geoip_info.addr);

		return;
	}

	if (geoip_info.type == GEOIP_COUNTRY_EDITION)
		DPRINTF("%s: IPv%d: %s, Type: %d, Country: %s\n", __FUNCTION__, geoip_info.ipv6 ? 6 : 4,
			geoip_info.addr, geoip_info.type, geoip_info.country_code);
        else
                DPRINTF("%s: IPv%d: %s, Type: %d, Country: %s, Region: %s, City: %s, Postal Code: %s, Lat: %f, Long: %f\n",
                        __FUNCTION__, geoip_info.ipv6 ? 6 : 4, geoip_info.addr, geoip_info.type, geoip_info.country_code,
                        geoip_info.region, geoip_info.city, geoip_info.postal_code, geoip_info.geo_lat, geoip_info.geo_long);
}
#endif

/* Shared memory functions */
char *format_size(unsigned long value)
{
	char tmp[16] = { 0 };

	if (value > 1048576)
		snprintf(tmp, sizeof(tmp), "%.2f MiB", (double)value / 1048576);
	else
	if (value > 1024)
		snprintf(tmp, sizeof(tmp), "%.2f kiB", (double)value / 1024);
        else
		snprintf(tmp, sizeof(tmp), "%u B", (unsigned int)value);

	return strdup(tmp);
}

unsigned long calculate_shared_memory_allocation(void)
{
	unsigned int sPids = sizeof(tPids);
	unsigned int sHosting = sizeof(tHosting);
	unsigned int tsPids;
	unsigned int tsHosting;
	unsigned long totalMem;

	DPRINTF("Shared memory requirements:%c", '\n');
	DPRINTF("===========================%c", '\n');

	DPRINTF("%c", '\n');
	DPRINTF("Structure lengths:%c", '\n');
	DPRINTF("\tPids: %u bytes (reason string length is %d bytes)\n", sPids, MAX_PID_REASON);
	DPRINTF("\tHosting information: %u bytes\n", sHosting);

	DPRINTF("%c", '\n');
	DPRINTF("Limits:%c", '\n');
	DPRINTF("\tPids: %u\n", MAX_PIDS);
	DPRINTF("\tHosting information: %d\n", MAX_HOSTING);

	tsPids = MAX_PIDS * sPids;
	tsHosting = MAX_HOSTING * sHosting;
	totalMem = tsPids + tsHosting;

	DPRINTF("%c", '\n');
	DPRINTF("Total structure sizes:%c", '\n');
	DPRINTF("\tPids: %s\n", format_size(tsPids));
	DPRINTF("\tHosting information: %s\n", format_size(tsHosting));

	DPRINTF("%c", '\n');
	DPRINTF("Total memory required: %s\n", format_size(totalMem));

	return totalMem;
}

tShared *shared_mem_setup(void)
{
	shmid = -1;
	shared_memory = (void *)0;
	int key = parent_pid;

	shmid = shmget((key_t)key, sizeof(tShared), 0666 | IPC_CREAT);
	if (shmid == -1) {
		DPRINTF("%s: Cannot allocate shared memory, errno %d (%s)\n", __FUNCTION__,
			errno, strerror(errno));
		return NULL;
	}

	shared_memory = shmat(shmid, (void *)0, 0);
	if (shared_memory == (void *)-1) {
		DPRINTF("%s: Cannot attach shared memory, errno %d (%s)\n", __FUNCTION__,
			errno, strerror(errno));
		return NULL;
	}

	return (tShared *) shared_memory;
}

void shared_mem_free(void)
{
	if (shmdt(shared_memory) == -1)
		DPRINTF("%s: shmdt failed\n", __FUNCTION__);

	if (shmctl(shmid, IPC_RMID, 0) == -1)
		DPRINTF("%s: shmctl(IPC_RMID) failed\n", __FUNCTION__);

	shared_memory = NULL;
	DPRINTF("%s: Done\n", __FUNCTION__);
}

int shared_mem_init(void)
{
	shared_mem = shared_mem_setup();
	if (shared_mem == NULL) {
		DPRINTF("%s: Cannot setup shared memory\n", __FUNCTION__);
		return -1;
	}

        return 0;
}

int shared_mem_init_first(void)
{
	int ret = shared_mem_init();

	if (ret == 0) {
		shared_mem->_num_pids = 0;
		shared_mem->_num_hosting = 0;
		memset(shared_memory, 0, sizeof(tShared));

		DPRINTF("%s: Resetting shared memory\n", __FUNCTION__);
	}

	return ret;
}

int shared_mem_check(void)
{
	if (shared_mem == NULL) {
		if (shared_mem_init() < 0) {
			DPRINTF("%s: Cannot shared get memory\n", __FUNCTION__);
			return -ENOMEM;
		}
	}

	return 0;
}

/* Hosting database functions */
void utils_hosting_add(pid_t pid, char *ip, char *hostname, char *host, char *path, char *browser)
{
	int num;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return;
	}

	if (shared_mem->_num_hosting + 1 > sizeof(shared_mem->_hosting)) {
		DPRINTF("%s: No space to add a new PID information\n", __FUNCTION__);
		return;
	}

	ip = get_ip_address(ip, NULL);
	DPRINTF("%s: IP address is '%s' [hostname is %s]\n", __FUNCTION__, ip, hostname);

	num = shared_mem->_num_hosting;
	#ifdef USE_GEOIP
	char *tmp = project_info_get("geoip_enable");
	if (get_boolean(tmp)) {
		char *fn = project_info_get("geoip_file");
		if (fn != NULL) {
			char *exp = project_info_get("geoip_expose");
			shared_mem->_hosting[num].geoip = geoip_get_info(fn, ip);
			geoip_info_dump(shared_mem->_hosting[num].geoip);

			if (exp != NULL) {
				int i;
				tTokenizer t;

				t = tokenize(exp, " ");
				for (i = 0; i < t.numTokens; i++) {
					if (strcmp(t.tokens[i], "COUNTRY") == 0) {
						if (shared_mem->_hosting[num].geoip.type == -1)
							variable_add_fl("GEOIP_COUNTRY", "Private or unknown", TYPE_MODULE, 0, TYPE_STRING,
								VARFLAG_READONLY);
						else
							variable_add_fl("GEOIP_COUNTRY", shared_mem->_hosting[num].geoip.country_code,
								TYPE_MODULE, -1, TYPE_STRING, VARFLAG_READONLY);
					}
					else
					if (strcmp(t.tokens[i], "REGION") == 0) {
						if ((shared_mem->_hosting[num].geoip.type == -1)
							|| (shared_mem->_hosting[num].geoip.type != GEOIP_CITY_EDITION_REV1))
							variable_add_fl("GEOIP_REGION", "Unknown", TYPE_MODULE, 0, TYPE_STRING,
								VARFLAG_READONLY);
						else
							variable_add_fl("GEOIP_REGION", shared_mem->_hosting[num].geoip.region,
								TYPE_MODULE, -1, TYPE_STRING, VARFLAG_READONLY);
					}
					else
					if (strcmp(t.tokens[i], "CITY") == 0) {
						if ((shared_mem->_hosting[num].geoip.type == -1)
							|| (shared_mem->_hosting[num].geoip.type != GEOIP_CITY_EDITION_REV1))
							variable_add_fl("GEOIP_CITY", "Unknown", TYPE_MODULE, 0, TYPE_STRING,
								VARFLAG_READONLY);
						else
							variable_add_fl("GEOIP_CITY", shared_mem->_hosting[num].geoip.city,
								TYPE_MODULE, -1, TYPE_STRING, VARFLAG_READONLY);
					}
					else
					if (strcmp(t.tokens[i], "POSTAL_CODE") == 0) {
						if ((shared_mem->_hosting[num].geoip.type == -1)
							|| (shared_mem->_hosting[num].geoip.type != GEOIP_CITY_EDITION_REV1))
							variable_add_fl("GEOIP_POSTAL_CODE", "Unknown", TYPE_MODULE, 0, TYPE_STRING,
								VARFLAG_READONLY);
						else
							variable_add_fl("GEOIP_POSTAL_CODE",  shared_mem->_hosting[num].geoip.postal_code,
								TYPE_MODULE, -1, TYPE_STRING, VARFLAG_READONLY);
					}
					else
					if (strcmp(t.tokens[i], "LONGITUDE") == 0) {
						if ((shared_mem->_hosting[num].geoip.type == -1)
							|| (shared_mem->_hosting[num].geoip.type != GEOIP_CITY_EDITION_REV1))
							variable_add_fl("GEOIP_LONGITUDE",  "Unknown", TYPE_MODULE, 0, TYPE_STRING,
								VARFLAG_READONLY);
						else {
							char tmp[16] = { 0 };
							snprintf(tmp, sizeof(tmp), "%f", shared_mem->_hosting[num].geoip.geo_long);

							variable_add_fl("GEOIP_LONGITUDE", tmp,	TYPE_MODULE, -1, TYPE_DOUBLE,
								VARFLAG_READONLY);
						}
					}
					else
					if (strcmp(t.tokens[i], "LATITUDE") == 0) {
						if ((shared_mem->_hosting[num].geoip.type == -1)
							|| (shared_mem->_hosting[num].geoip.type != GEOIP_CITY_EDITION_REV1))
							variable_add_fl("GEOIP_LATITUDE",  "Unknown", TYPE_MODULE, 0, TYPE_STRING,
								VARFLAG_READONLY);
						else {
							char tmp[16] = { 0 };
							snprintf(tmp, sizeof(tmp), "%f", shared_mem->_hosting[num].geoip.geo_lat);

							variable_add_fl("GEOIP_LATITUDE", tmp, TYPE_MODULE, -1, TYPE_DOUBLE,
								VARFLAG_READONLY);
					}
					}
					else
						DPRINTF("%s: Cannot expose '%s'. Unknown GeoIP value\n", __FUNCTION__, t.tokens[i]);
				}

				free_tokens(t);
			}

			utils_free("utils.utils_hosting_add.exp", exp);
		}
		utils_free("utils.utils_hosting_add.fn", fn);
	}
	else
		strncpy(shared_mem->_hosting[num].geoip.addr, ip, sizeof(shared_mem->_hosting[num].geoip.addr));
	utils_free("utils.utils_hosting_add.tmp", tmp);
	#else
	strncpy(shared_mem->_hosting[num].addr, ip, sizeof(shared_mem->_hosting[num].addr));
	#endif
	shared_mem->_hosting[num].pid = pid;
	strncpy(shared_mem->_hosting[num].host, host, sizeof(shared_mem->_hosting[num].host));
	strncpy(shared_mem->_hosting[num].path, path, sizeof(shared_mem->_hosting[num].path));
	strncpy(shared_mem->_hosting[num].browser, browser, sizeof(shared_mem->_hosting[num].browser));
	shared_mem->_num_hosting = num + 1;

	DPRINTF("%s: PID #%d added to shared memory with hosting information for %s [#%d]\n",
		__FUNCTION__, pid, ip, shared_mem->_num_hosting);
}

void utils_hosting_delete(pid_t pid)
{
	int i, num;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return;
	}

	num = shared_mem->_num_hosting;
	for (i = 0; i < shared_mem->_num_hosting; i++)
		if (shared_mem->_hosting[i].pid == pid) {
			shared_mem->_hosting[i].pid = shared_mem->_hosting[num - 1].pid;
			strcpy(shared_mem->_hosting[i].path, shared_mem->_hosting[num - 1].path);
			strcpy(shared_mem->_hosting[i].host, shared_mem->_hosting[num - 1].host);
			strcpy(shared_mem->_hosting[i].browser, shared_mem->_hosting[num - 1].browser);

			shared_mem->_hosting[num - 1].pid = 0;
			memset(shared_mem->_hosting[num - 1].host, 0, sizeof(shared_mem->_hosting[num - 1].host));
			memset(shared_mem->_hosting[num - 1].path, 0, sizeof(shared_mem->_hosting[num - 1].path));
			memset(shared_mem->_hosting[num - 1].browser, 0, sizeof(shared_mem->_hosting[num - 1].browser));
			num--;
	}

	shared_mem->_num_hosting = num;
}

void utils_hosting_dump(void)
{
	int i;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return;
	}

	for (i = 0; i < shared_mem->_num_hosting; i++) {
	#ifdef USE_GEOIP
		tGeoIPInfo gip = shared_mem->_hosting[i].geoip;

		dump_printf("Process #%d: PID #%d, IP '%s', host '%s', path '%s', browser is '%s'\n",
			i + 1, shared_mem->_hosting[i].pid, shared_mem->_hosting[i].geoip.addr,
			shared_mem->_hosting[i].host, shared_mem->_hosting[i].path,
			shared_mem->_hosting[i].browser);

		if (gip.type == GEOIP_COUNTRY_EDITION)
			dump_printf("\tGeoIP = { country: %s }\n",
				gip.country_code);
		else
		if (gip.type != -1)
			dump_printf("\tGeoIP = { country: %s, region: %s, city: %s, postal code: %s, latitude: %f, longitude: %f }\n",
				gip.country_code, gip.region, gip.city, gip.postal_code, gip.geo_lat, gip.geo_long);
	#else
		dump_printf("Process #%d: PID #%d, IP '%s', host '%s', path '%s', browser '%s'\n",
			i + 1, shared_mem->_hosting[i].pid, shared_mem->_hosting[i].addr,
			shared_mem->_hosting[i].host, shared_mem->_hosting[i].path,
			shared_mem->_hosting[i].browser);
	#endif
	}
}

/* PID functions */
void utils_pid_add(pid_t pid, char *reason)
{
	int num;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return;
	}

	if (shared_mem->_num_pids + 1 > sizeof(shared_mem->_pids)) {
		DPRINTF("%s: No space to add a new PID information\n", __FUNCTION__);
		return;
	}

	if (utils_pid_exists_in_system(pid) == 0) {
		DPRINTF("%s: PID #%d doesn't exist in the system\n", __FUNCTION__, pid);
		return;
	}

	num = shared_mem->_num_pids;
	shared_mem->_pids[num].pid = pid;
	if (reason != NULL)
		strncpy(shared_mem->_pids[num].reason, reason, sizeof(shared_mem->_pids[num].reason));
	else
		strcpy(shared_mem->_pids[num].reason, "unknown");

	shared_mem->_num_pids = num + 1;

	DPRINTF("%s: PID #%d added to the shared memory [#%d, reason is '%s']\n", __FUNCTION__,
		pid, shared_mem->_num_pids, reason);

/*
	if (_pids == NULL) {
		_pids = (tPids *)utils_alloc( "utils.utils_pid_add", sizeof(tPids) );
		_pids_num = 0;
	}
	else
		_pids = (tPids *)realloc( _pids, (_pids_num + 1) * sizeof(tPids) );

	_pids[ _pids_num ].pid = pid;
	_pids[ _pids_num ].reason = strdup(reason);
	_pids_num++;
*/
}

void utils_pid_delete(pid_t pid)
{
	int i, num;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return;
	}

	if (shared_mem == NULL)
		return;

	num = shared_mem->_num_pids;
	for (i = 0; i < shared_mem->_num_pids; i++)
		if (shared_mem->_pids[i].pid == pid) {
			shared_mem->_pids[i].pid = shared_mem->_pids[num - 1].pid;
			strcpy(shared_mem->_pids[i].reason, shared_mem->_pids[num - 1].reason);

			shared_mem->_pids[num - 1].pid = 0;
			memset(shared_mem->_pids[num - 1].reason, 0, sizeof(shared_mem->_pids[num - 1].reason));
			num--;
		}

	shared_mem->_num_pids = num;
}

int utils_pid_exists(pid_t pid)
{
	int i;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return -1;
	}

	for (i = 0; i < shared_mem->_num_pids; i++)
		if (shared_mem->_pids[i].pid == pid)
			return 1;

	return 0;
}

int utils_pid_exists_in_system(pid_t pid)
{
	char tmp[256] = { 0 };

	snprintf(tmp, sizeof(tmp), "/proc/%d/status", pid);
	return (access(tmp, F_OK) == 0);
}

int utils_pid_num_free(void)
{
	int num;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return -1;
	}

	num = shared_mem->_num_pids;
	return MAX_PIDS - num;
}

int utils_hosting_num_free(void)
{
	int num;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return -1;
	}

	num = shared_mem->_num_hosting;
	return MAX_HOSTING - num;
}

int utils_pid_get_num_with_reason(char *reason)
{
	int i, num = 0;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return -1;
	}

	for (i = 0; i < shared_mem->_num_pids; i++)
		if (strcmp(shared_mem->_pids[i].reason, reason) == 0)
			num++;

	return num;
}

int utils_pid_get_host_clients(char *host)
{
	char tmp[1024] = { 0 };

	snprintf(tmp, sizeof(tmp), "Hosting:%s", host);
	return utils_pid_get_num_with_reason(tmp);
}

void utils_pid_dump(void)
{
	int i;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return;
	}

	for (i = 0; i < shared_mem->_num_pids; i++)
		dump_printf("PID #%d: %d (reason: %s)\n", i + 1, shared_mem->_pids[i].pid,
			shared_mem->_pids[i].reason);

/*
	if (_pids == NULL)
		return;

	for (i = 0; i < _pids_num; i++)
		dump_printf("PID #%d: %d (reason: %s)\n", i + 1, _pids[i].pid, _pids[i].reason);
*/
}

int utils_pid_signal_all(int sig)
{
	int i;
	pid_t pid;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return -1;
	}

	for (i = 0; i < shared_mem->_num_pids; i++) {
		pid = shared_mem->_pids[i].pid;

		/* Don't send to controll process itself */
		if (pid != parent_pid) {
			DPRINTF("%s: Sending signal %d to PID %d\n",
				__FUNCTION__, sig, pid);
			kill(pid, sig);
		}
		else
			DPRINTF("%s: Ignoring parent process (PID #%d)\n",
				__FUNCTION__, pid );
	}

	return shared_mem->_num_pids;

/*
	if (_pids == NULL)
		return 0;

	for (i = 0; i < _pids_num; i++) {
		DPRINTF("%s: Sending signal %d to PID %d\n",
			__FUNCTION__, sig, _pids[i].pid);
		kill(_pids[i].pid, sig);
	}

	return _pids_num;
*/

}

int utils_pid_wait_all(void)
{
	int i, new, old;

	if (shared_mem_check() < 0) {
		DPRINTF("%s: Cannot get shared memory\n", __FUNCTION__);
		return -1;
	}

	new = shared_mem->_num_pids;
	old = new;
	for (i = 0; i < new; i++) {
		waitpid( shared_mem->_pids[i].pid, NULL, 0);
		new--;
	}

	return old;

/*
	if (_pids == NULL)
		return 0;

	new = _pids_num;
	old = new;
	for (i = 0; i < new; i++) {
		waitpid( _pids[i].pid, NULL, 0 );
		new--;
	}
	_pids_num = 0;

	return old;
*/
}

int utils_pid_kill_all(void)
{
	return utils_pid_signal_all(SIGKILL);
}

