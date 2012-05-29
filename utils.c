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

int initialize(void)
{
	int err;

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

	#ifdef USE_INTERNAL_DB
	idb_free();
	#endif
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
	project_info.name = NULL;
	project_info.admin_name = NULL;
	project_info.admin_mail = NULL;
	project_info.file_config = NULL;
	project_info.dir_defs = NULL;
	project_info.dir_views = NULL;

	project_info.host_http = NULL;
	project_info.host_secure = NULL;
	project_info.cert_dir = NULL;
	project_info.cert_root = NULL;
	project_info.cert_pk = NULL;
	project_info.cert_pub = NULL;
}

void project_info_fill(void)
{
	project_info.name = strdup( config_get("project", "name") );
	project_info.admin_name = strdup( config_get("project", "administrator.name") );
	project_info.admin_mail = config_get("project", "administrator.email");
	project_info.file_config = config_get("project", "config.file");
	project_info.dir_defs = config_get("project", "config.directory.definitions");
	project_info.dir_views = config_get("project", "config.directory.views");

	project_info.host_http = config_get("host", "http");
	project_info.host_secure = config_get("host", "secure");
	project_info.cert_dir = config_get("host", "certificate.dir");
	project_info.cert_root = config_get("host", "certificate.root");
	project_info.cert_pk = config_get("host", "certificate.private");
	project_info.cert_pub = config_get("host", "certificate.public");

	DPRINTF("%s: Project information structure set\n", __FUNCTION__);
}

void project_info_dump(void)
{
	int num = 0;

	printf("\nProject hosting information dump:\n");

	if (project_info.host_http != NULL) {
		printf("\tHTTP host: '%s'\n", project_info.host_http);
		num++;
	}
	if (project_info.host_secure != NULL) {
		printf("\tHTTPS host: '%s'\n", project_info.host_secure);
		num++;
	}
	if (project_info.cert_dir != NULL) {
		printf("\tHTTPS certificate directory: '%s'\n", project_info.cert_dir);
		num++;
	}
	if (project_info.cert_root != NULL) {
		printf("\tHTTPS root certificate: '%s'\n", project_info.cert_root);
		num++;
	}
	if ((project_info.cert_pk != NULL) && (project_info.cert_pub != NULL)) {
		printf("\tHTTPS certificates: private '%s', public '%s'\n",
			project_info.cert_pk, project_info.cert_pub);
		num++;
	}

	printf("\nProject information dump:\n");

	if (project_info.name != NULL) {
		printf("\tProject name: '%s'\n", project_info.name);
		num++;
	}
	if (project_info.admin_name != NULL) {
		printf("\tProject administrator: '%s'\n", project_info.admin_name);
		num++;
	}
	if (project_info.admin_mail != NULL) {
		printf("\tProject administrator's e-mail: '%s'\n", project_info.admin_mail);
		num++;
	}
	if (project_info.file_config != NULL) {
		printf("\tProject config file: '%s'\n", project_info.file_config);
		num++;
	}
	if (project_info.dir_defs != NULL) {
		printf("\tProject definitions dir: '%s'\n", project_info.dir_defs);
		num++;
	}
	if (project_info.dir_views != NULL) {
		printf("\tProject views dir: '%s'\n", project_info.dir_views);
		num++;
	}

	if (num == 0)
		printf("\tNo project data available\n");

	printf("\n");
}

char *project_info_get(char *type)
{
	if ((project_info.name != NULL) && (strcmp(type, "name") == 0))
		return strdup(project_info.name);
	if ((project_info.admin_name != NULL)  && (strcmp(type, "admin_name") == 0))
		return strdup(project_info.admin_name);
	if ((project_info.admin_mail != NULL) && (strcmp(type, "admin_mail") == 0))
		return strdup(project_info.admin_mail);
	if ((project_info.file_config != NULL) && (strcmp(type, "file_config") == 0))
		return strdup(project_info.file_config);
	if ((project_info.dir_defs != NULL) && (strcmp(type, "dir_defs") == 0))
		return strdup(project_info.dir_defs);
	if ((project_info.dir_views != NULL) && (strcmp(type, "dir_views") == 0))
		return strdup(project_info.dir_views);
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

	return NULL;
}

void project_info_cleanup(void)
{
	free(project_info.name);
	free(project_info.admin_name);
	free(project_info.admin_mail);
	free(project_info.file_config);
	free(project_info.dir_defs);
	free(project_info.dir_views);
	free(project_info.host_http);
	free(project_info.host_secure);
	free(project_info.cert_dir);
	free(project_info.cert_root);
	free(project_info.cert_pk);
	free(project_info.cert_pub);

	/* To set to NULLs */
	project_info_init();
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
	if (ret != 0)
		cleanup();

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

					if ((host_http != NULL)
						&& ((strcmp(host_http, host) == 0)
							|| (strcmp(host_secure, host) == 0))) {
						project_dump();
						ret = 0;
						break;
					}

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

	if (DEBUG_TRIM)
		DPRINTF("%s: Trimming string '%s'\n", __FUNCTION__, str);

        if (strchr(str, ' ') != NULL)
                while (*str == ' ')
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

