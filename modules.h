#ifndef CDVWS_MODULES_H
#define CDVWS_MODULES_H

#define TYPE_INT        0x01
#define TYPE_LONG       0x02
#define TYPE_DOUBLE     0x04
#define TYPE_STRING     0x08
#define TYPE_ARRAY      0x10
#define TYPE_STRUCT     0x20

#define TYPE_BASE	0x01
#define TYPE_QGET       TYPE_BASE + 0x01
#define TYPE_QPOST      TYPE_BASE + 0x02
#define TYPE_QSCRIPT    TYPE_BASE + 0x04
#define TYPE_MODULE	TYPE_BASE + 0x08
#define TYPE_MODAUTH	TYPE_BASE + 0x10

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

#define	BUFSIZE		8192

typedef struct tTokenizer {
        char **tokens;
        int numTokens;
} tTokenizer;

tTokenizer tokenize(char *string, char *by);
void free_tokens(tTokenizer t);

/* Variable manipulation stuff */
extern int variable_add(char *name, char *value, int q_type, int idParent, int type);
int variable_lookup_name_idx(char *name, char *type, int idParent);
void variable_dump(void);
void variable_free_all(void);
char *variable_get_element_as_string(char *el, char *type);
int variable_get_type(char *el, char *type);
char *variable_get_type_string(char *el, char *type);
int variable_allow_overwrite(char *name, int allow);
int variable_get_overwrite(char *name);
int variable_set_deleted(char *name, int is_deleted);
int variable_get_deleted(char *name);
int variable_get_idx(char *el, char *type);
int variable_set_fixed_type(char *name, char *type);
int variable_get_fixed_type(char *name);
int variable_create(char *name, char *type);

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
int first_initialize(int enabled);
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
int dump_file_is_set(void);
void dump_unset_file(void);
struct timespec utils_get_time(int diff);
void utils_pid_add(int pid);
void utils_pid_dump(void);
int utils_pid_kill_all(void);
int utils_pid_wait_all(void);
int utils_pid_signal_all(int sig);
char *replace(char *str, char *what, char *with);
int cdvPrintfAppend(char *var, int max_len, const char *fmt, ...);
char *cdvStringAppend(char *var, char *val);
int gettype(char *val);
int is_numeric(char *val);
int get_boolean(char *val);
int is_string(char *val);
int is_comment(char *val);
int get_type_from_string(char *type, int allow_autodetection);
char *get_type_string(int id);

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
int xml_load_opt(char *xmlFile, char *root);
char **xml_get_all(char *nodename, int *oNum);
void xml_free_all(char **ret, int num);
int xml_query(char *xmlFile, char *xPath);
char *xml_get(char *basefile, char *node, char *name, char *value, char *out_value);
void xml_dump(void);
int xml_cleanup(void);

/* XmlRPC related stuff */
char *xmlrpc_process(char *xml);

/* RegEx stuff */
int regex_parse(char *xmlFile);
char *regex_get(char *expr);
int regex_exists(char *expr);
void regex_dump(void);
void regex_free(void);
int regex_get_idx(char *str);
void regex_dump_matches(char **elements, int num_elems);
void regex_free_matches(char **elements, int num_elems);
char **regex_get_matches(char *str, int *num_matches);
char *regex_format_new_string(char *str);
int regex_match(char *expr, char *str);

/* General database stuff */
char *database_format_query(char *xmlFile, char *table, char *type);

#endif
