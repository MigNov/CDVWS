#ifndef CDVWS_H
#define CDVWS_H

#define USE_INTERNAL_DB

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <inttypes.h>

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

typedef struct tConfigVariable {
	char *filename;
	char *space;
	char *key;
	char *value;
	char *fullkey;
} tConfigVariable;

typedef struct tProjectInformation {
	char *name;
	char *admin_name;
	char *admin_mail;
	char *file_config;
	char *dir_defs;
	char *dir_views;
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

tProjectInformation project_info;

char *basedir;
tConfigVariable *configVars;
int numConfigVars;

char *trim(char *str);
int ensure_directory_existence(char *dir);

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

#define TDS_ROW(x)		tds.rows[x]
#define TDS_FIELD_CNT(x)	tds.rows[x].num_fields
#define TDS_ROW_FIELD(x, y)	tds.rows[x].tdi[y]
#define TDS_LAST_ROW(x)		TDS_ROW_FIELD(x, TDS_FIELD_CNT(x) - 1)

// tTableDataSelect tds
// tds[0].row[0].name
// tTableDataSelect -> tTableDataRow -> tTableDataField

long _idb_table_id(char *name);

tTableDef *idb_tables;
tTableFieldDef *idb_fields;
tTableData *idb_tabdata;

int idb_tables_num;
int idb_fields_num;
int idb_tabdata_num;
char *_idb_filename;
char *_idb_datadir;
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
int idb_create_table(char *name, int num_fields, tTableFieldDef *fields, char *comment);
int idb_table_insert(char *name, int num_data, tTableDataInput *td);
int idb_table_update(char *table_name, long idRow, int num_data, tTableDataInput *td);
int idb_table_delete(char *table_name, long idRow);
tTableDataSelect idb_table_select(char *table, int num_fields, char **fields, int num_where_fields, tTableDataInput *where_fields);
void idb_dump(void);
int idb_type_id(char *type);
char *idb_get_filename(void);
int idb_process_query(char *query);

/* Config stuff */
int config_initialize(void);
int config_variable_add(char *filename, char *space, char *key, char *value, char *fullkey);
char *config_variable_get(char *space, char *key);
int config_variable_get_idx(char *space, char *key);
char *config_get(char *space, char *key);
int config_load(char *filename, char *space);
char* config_read(const char *filename, char *key);
void config_free(void);

/* Definitions stuff */
int definitions_initialize(void);
void definitions_cleanup(void);
int definitions_load_directory(char *dir);

/* Utils stuff */
int initialize(void);
void cleanup(void);
tTokenizer tokenize(char *string, char *by);
void free_tokens(tTokenizer t);
char *trim(char *str);
char *get_full_path(char *basedir);
char *process_handlers(char *path);
off_t get_file_size(char *filename);
unsigned char *data_fetch(int fd, int len, long *size, int extra);
int load_project_directory(char *project_path);
int load_project(char *project_file);

/* Project related options */
void project_info_init(void);
void project_info_fill(void);
char *project_info_get(char *type);
void project_info_dump(void);
void project_info_cleanup(void);

/* Module stuff */
char *_modules_directory;

/* Xml stuff */
char *xml_get(char *basefile, char *node, char *name, char *value, char *out_value);

/* Database stuff */
char *database_format_query(char *xmlFile, char *table, char *type);

#endif
