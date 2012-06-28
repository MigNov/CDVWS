#ifndef CDVWS_H
#define CDVWS_H

#define VERSION		"0.0.1"

/* For now it doesn't work without internal DB */
#define USE_INTERNAL_DB
#define USE_SSL

#include <time.h>
#include <stdio.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <signal.h>
#include <stdlib.h>
#include <inttypes.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

/* Stdin file descriptor */
#define STDIN		1

#ifdef USE_READLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#ifdef USE_MINCRYPT
/* We cannot include mincrypt.h because of re-introducing definitions */
void mincrypt_set_password(char *salt, char *password, int vector_multiplier);
int mincrypt_set_encoding_type(int type);
void mincrypt_dump_vectors(char *dump_file);
int mincrypt_read_key_file(char *keyfile, int *oIsPrivate);
void mincrypt_cleanup(void);
unsigned char *mincrypt_encrypt(unsigned char *block, size_t size, int id, size_t *new_size);
unsigned char *mincrypt_decrypt(unsigned char *block, size_t size, int id, size_t *new_size, int *read_size);
int mincrypt_encrypt_file(char *filename1, char *filename2, char *salt, char *password, int vector_multiplier);
int mincrypt_decrypt_file(char *filename1, char *filename2, char *salt, char *password, int vector_multiplier);
int mincrypt_generate_keys(int bits, char *salt, char *password, char *key_private, char *key_public);
long mincrypt_get_version(void);
int mincrypt_set_simple_mode(int enable);
unsigned char *mincrypt_convert_to_four_system(unsigned char *data, int len);
unsigned char *mincrypt_convert_from_four_system(unsigned char *data, int len);
int mincrypt_set_four_system_quartet(char *quartet);
char *mincrypt_get_four_system_quartet(void);
unsigned char *mincrypt_base64_encode(const char *in, size_t *size);
unsigned char *mincrypt_base64_decode(const char *in, size_t *size);
#endif

#ifdef USE_SSL
#define TCP_BUF_SIZE	1048576
#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef int (tProcessRequest)(SSL *ssl, BIO *io, int connected, struct sockaddr_in client_addr, char *buf, int len);

SSL_CTX *init_ssl_layer(char *private_key, char *public_key, char *root_key);
int _sockets_done;
int accept_loop(SSL_CTX *ctx, int sock, tProcessRequest req);
int _tcp_in_progress;
char _tcp_buf[TCP_BUF_SIZE];
int _tcp_total;
#endif

#define	BUFSIZE		8192

#ifdef USE_INTERNAL_DB
#define IDB_TABLES	0x01
#define IDB_FIELDS	0x02
#define IDB_TABDATA	0x04

#define	IDB_TYPE_INT	0x01
#define	IDB_TYPE_LONG	0x02
#define	IDB_TYPE_STR	0x04
#define	IDB_TYPE_FILE	0x08
#endif

typedef struct tTokenizer {
        char **tokens;
        int numTokens;
} tTokenizer;

tTokenizer tokenize(char *string, char *by);
void free_tokens(tTokenizer t);

int *_pids;
int _pids_num;

typedef struct tConfigVariable {
	char *filename;
	char *space;
	char *key;
	char *value;
	char *fullkey;
} tConfigVariable;

typedef struct tProjectInformation {
	char *name;
	char *root_dir;
	char *admin_name;
	char *admin_mail;
	char *file_config;
	char *dir_defs;
	char *dir_views;
	char *dir_files;
	char *host_http;
	char *host_secure;
	char *cert_dir;
	char *cert_root;
	char *cert_pk;
	char *cert_pub;
} tProjectInformation;

typedef struct tAttr {
        char *filename;
        char *name;
        char *node;
        char *value;
        int numIter;
} tAttr;

int xml_numIter;
int xml_numAttr;
tAttr *xattr;

char *_proj_root_dir;
tProjectInformation project_info;

char *basedir;
tConfigVariable *configVars;
int numConfigVars;
int _shell_project_loaded;
int _shell_enabled;
FILE *_dump_fp;

char *trim(char *str);
int ensure_directory_existence(char *dir);
float get_time_float_us(struct timespec ts, struct timespec te);
char *_shell_history_file;

#define TIME_CURRENT	0
#define TIME_DIFF	1

#define READLINE_HISTORY_FILE_CDV	"~/.cdv_history"
#define READLINE_HISTORY_FILE_IDB	"~/.cdv_idb_history"

#define SHELL_IS_REMOTE(io, fd)		(!((io == NULL) && (fd == STDIN)))
#define SHELL_OVER_SSL(io, fd)		(SHELL_IS_REMOTE(io, fd) && (io != NULL))

#define timespecsub(a, b, result)					\
	do {								\
		(result)->tv_sec = (a)->tv_sec - (b)->tv_sec;		\
		(result)->tv_nsec = (a)->tv_nsec - (b)->tv_nsec;	\
		if ((result)->tv_nsec < 0) {				\
			--(result)->tv_sec;				\
			(result)->tv_nsec += 1000000000;		\
		}							\
	} while (0)

#define HTTP_CODE_OK		200
#define HTTP_CODE_BAD_REQUEST	400
#define HTTP_CODE_UNAUTHORIZED	401
#define HTTP_CODE_FORBIDDEN	403
#define HTTP_CODE_NOT_FOUND	404
#define HTTP_CODE_NOT_ALLOWED	405

#ifdef USE_INTERNAL_DB
typedef struct tTableDef {
	long id;
	char *name;
	char *comment;
} tTableDef;

typedef struct tTableFieldDef {
	long id;
	long idTable;
	char *name;
	int type;
} tTableFieldDef;

typedef struct tTableData {
	long id;
	long idField;
	long idRow;
	int iValue;
	long lValue;
	char *sValue;
} tTableData;

typedef struct tTableDataInput {
	char *name;
	int iValue;
	long lValue;
	char *sValue;
	void *cData;
	long cData_len;
} tTableDataInput;

typedef struct tTableDataField {
	char *name;
	int type;
	int iValue;
	long lValue;
	char *sValue;
	void *cData;
	long cData_len;
} tTableDataField;

typedef struct tTableDataRow {
	long num_fields;
	tTableDataField *tdi;
} tTableDataRow;

typedef struct tTableDataSelect {
	long num_rows;
	tTableDataRow *rows;
} tTableDataSelect;

tTableDataSelect tdsNone;
tTableDataSelect _last_tds;

tTableDataSelect idb_get_last_select_data(void);

#define TDS_ROW(x)		tds.rows[x]
#define TDS_FIELD_CNT(x)	tds.rows[x].num_fields
#define TDS_ROW_FIELD(x, y)	tds.rows[x].tdi[y]
#define TDS_LAST_ROW(x)		TDS_ROW_FIELD(x, TDS_FIELD_CNT(x) - 1)

tTableDef *idb_tables;
tTableFieldDef *idb_fields;
tTableData *idb_tabdata;

int _idb_num_queries;
int _idb_num_queries_create;
int _idb_num_queries_drop;
int _idb_num_queries_insert;
int _idb_num_queries_update;
int _idb_num_queries_delete;
int _idb_num_queries_select;

int _idb_mincrypt_enabled;

int idb_tables_num;
int idb_fields_num;
int idb_tabdata_num;
int _idb_num_queries;
char *_idb_filename;
char *_idb_datadir;
char *_idb_querylog;
struct timespec _idb_last_ts;
struct timespec _idb_session_start;
#else
typedef void tTableDataSelect;
#endif

#define UINT32STR(var, val)     \
	var[3] = (val >> 24) & 0xff;    \
	var[2] = (val >> 16) & 0xff;    \
	var[1] = (val >>  8) & 0xff;    \
	var[0] = (val      ) & 0xff;

#define BYTESTR(var, val)       \
	var[0] =  val;

#define WORDSTR(var, val)       \
	var[1] = (val >> 8) & 0xff;     \
	var[0] = (val     ) & 0xff;

#define GETBYTE(var)	(var[0])
#define GETWORD(var)	((var[1] << 8) + (var[0]))
#define GETUINT32(var)	(uint32_t)(((uint32_t)var[3] << 24) + ((uint32_t)var[2] << 16) + ((uint32_t)var[1] << 8) + ((uint32_t)var[0]))

/* Internal database stuff */
int idb_init(void);
void idb_free(void);
long idb_table_id(char *name);
int idb_table_create(char *name, int num_fields, tTableFieldDef *fields, char *comment);
int idb_table_insert(char *name, int num_data, tTableDataInput *td);
int idb_table_update_row(char *table_name, long idRow, int num_data, tTableDataInput *td);
int idb_table_update(char *table_name, int num_data, tTableDataInput *td, int num_where_fields, tTableDataInput *where_fields);
int idb_table_delete_row(char *table_name, long idRow);
int idb_table_delete(char *table_name, int num_where_fields, tTableDataInput *where_fields);
tTableDataSelect idb_table_select(char *table, int num_fields, char **fields, int num_where_fields, tTableDataInput *where_fields);
void idb_dump(void);
int idb_type_id(char *type);
char *idb_get_filename(void);
int idb_query(char *query);
int idb_save(char *filename);
void idb_results_dump(tTableDataSelect tds);
void idb_results_free(tTableDataSelect *tds);
int idb_load(char *filename);
int idb_table_drop(char *table_name);
void idb_free_last_select_data(void);

/* Config stuff */
int config_initialize(void);
int config_variable_add(char *filename, char *space, char *key, char *value, char *fullkey);
char *config_variable_get(char *space, char *key);
int config_variable_get_idx(char *space, char *key);
char *config_get(char *space, char *key);
int config_load(char *filename, char *space);
char* config_read(const char *filename, char *key);
void config_variable_dump(char *space);
void config_free(void);

/* Definitions stuff */
int definitions_initialize(void);
void definitions_cleanup(void);
int definitions_load_directory(char *dir);

/* Utils stuff */
int initialize(void);
void cleanup(void);
void total_cleanup(void);
int first_initialize(void);
tTokenizer tokenize(char *string, char *by);
void free_tokens(tTokenizer t);
char *trim(char *str);
char *get_full_path(char *basedir);
char *process_handlers(char *path);
off_t get_file_size(char *filename);
unsigned char *data_fetch(int fd, int len, long *size, int extra);
int load_project_directory(char *project_path);
int load_project(char *project_file);
int data_write(int fd, const void *data, size_t len, long *size);
unsigned char *data_fetch(int fd, int len, long *size, int extra);
void project_dump(void);
char *get_mime_type(char *path);
int dump_set_file(char *filename);
void dump_printf(const char *fmt, ...);
void desc_printf(BIO *io, int fd, const char *fmt, ...);
char *desc_read(BIO *io, int fd);
int dump_file_is_set(void);
void dump_unset_file(void);
struct timespec utils_get_time(int diff);
void utils_pid_add(int pid);
void utils_pid_dump(void);
int utils_pid_kill_all(void);
int utils_pid_wait_all(void);
int utils_pid_signal_all(int sig);
char *replace(char *str, char *what, char *with);

/* Project related options */
void project_info_init(void);
void project_info_fill(void);
char *project_info_get(char *type);
void project_info_dump(void);
void project_info_cleanup(void);
int find_project_for_web(char *directory, char *host);

/* Module stuff */
char *_modules_directory;
int modules_initialize(void);
char *modules_get_directory(void);
int module_load(char *libname);

/* Xml stuff */
int xml_init(void);
int xml_load(char *xmlFile);
char **xml_get_all(char *nodename, int *oNum);
void xml_free_all(char **ret, int num);
int xml_query(char *xmlFile, char *xPath);
char *xml_get(char *basefile, char *node, char *name, char *value, char *out_value);
void xml_dump(void);
int xml_cleanup(void);

/* General database stuff */
char *database_format_query(char *xmlFile, char *table, char *type);

/* Sockets stuff */
int tcp_listen(int port);
int socket_has_data(int sfd, long maxtime);
int write_common(BIO *io, int sock, char *data, int len);
int run_server(int port, char *pk, char *pub, char *root_key);
int process_request_common(SSL *ssl, BIO *io, int connected, struct sockaddr_in client_addr, char *buf, int len);

/* MinCrypt wrapper stuff */
long wrap_mincrypt_get_version(void);
int wrap_mincrypt_set_password(char *salt, char *password);
unsigned char *wrap_mincrypt_encrypt(unsigned char *block, size_t size, int id, size_t *new_size);
unsigned char *wrap_mincrypt_decrypt(unsigned char *block, size_t size, int id, size_t *new_size, int *read_size);
int wrap_mincrypt_set_encoding_type(int type);
unsigned char *wrap_mincrypt_base64_encode(unsigned char *in, size_t *size);
unsigned char *wrap_mincrypt_base64_decode(unsigned char *in, size_t *size);
void wrap_mincrypt_cleanup(void);

/* Shell functions */
int run_shell(BIO *io, int cfd);
int process_shell_command(struct timespec ts, BIO *io, int cfd, char *str, char *ua, char *path);
int process_idb_command(struct timespec ts, BIO *io, int cfd, char *str);

#endif
