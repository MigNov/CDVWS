#define DEBUG_MAIN
#include "cdvws.h"

#define	DATADIR	"cdvdata/db"

#ifdef DEBUG_MAIN
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/main        ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int test_xml_query_data(void)
{
	int err;
	char *data = NULL;

	if (xml_init() < 0)
		return -1;

	data = database_format_query("/home/mig/Work/Interest/myPackages/cdvws/examples/article/definition/index.cdv", "article", "insert");
	printf("Insert query is '%s'\n", data);
	data = database_format_query("/home/mig/Work/Interest/myPackages/cdvws/examples/article/definition/index.cdv", "article", "update");
	printf("Update query is '%s'\n", data);
	data = database_format_query("/home/mig/Work/Interest/myPackages/cdvws/examples/article/definition/index.cdv", "article", "delete");
	printf("Delete query is '%s'\n", data);

	xml_cleanup();

	return 0;
}

int test_idb(void)
{
	int err;
	tTableFieldDef *fd = NULL;
	tTableData td = { 0 };
	tTableDataSelect tds = tdsNone;
	tTableDataInput *tdi = NULL;
	int i, num_fds = 5;
	char tmp[16] = { 0 };
	char **sel_fields = NULL;
	tTableDataInput *where_fields = NULL;

	fd = malloc( num_fds * sizeof(tTableFieldDef) );
	memset(fd, 0, num_fds * sizeof(tTableFieldDef));

	for (i = 0; i < num_fds; i++) {
		snprintf(tmp, sizeof(tmp), "field-%d", i + 1);
		fd[i].name = strdup(tmp);
		fd[i].type = (i % 2) ? IDB_TYPE_INT : IDB_TYPE_LONG;
	}

	if ((err = idb_create_table("table", num_fds, fd, NULL)) < 0) {
		printf("Error %d on idb_create_table\n", err);
		return -1;
	}

	tdi = malloc( num_fds * sizeof(tTableDataInput) );
	memset(tdi, 0, num_fds * sizeof(tTableDataInput) );

	for (i = 0; i < num_fds; i++) {
		snprintf(tmp, sizeof(tmp), "field-%d", i + 1);
		tdi[i].name = strdup(tmp);
		tdi[i].iValue = 10 * i;
		tdi[i].lValue = 10000 * i;
		tdi[i].sValue = strdup("test");
		tdi[i].cData = strdup("cdata");
		tdi[i].cData_len = 5;
		
	}

	idb_table_insert("table", num_fds, tdi);
	idb_table_insert("table", num_fds, tdi);

	num_fds -= 2;

	for (i = 0; i < num_fds; i++) {
		snprintf(tmp, sizeof(tmp), "field-tab2-%d", i + 1);
		fd[i].name = strdup(tmp);
		fd[i].type = (i % 2) ? IDB_TYPE_FILE : IDB_TYPE_STR;
	}

	if ((err = idb_create_table("table-2", num_fds, fd, "comment")) < 0) {
		printf("Error %d on idb_create_table\n", err);
		return -1;
	}
	free(fd);

	for (i = 0; i < num_fds; i++) {
		(tdi[i].iValue)++;
		(tdi[i].lValue)++;
		tdi[i].iValue *= 2000;
		tdi[i].lValue *= 10000;
	}

	DPRINTF("%s: Now updating row 2 of table\n", __FUNCTION__);
	idb_table_update("table", 2, num_fds, tdi);

	DPRINTF("%s: Now deleting row 1 of table\n", __FUNCTION__);
	idb_table_delete("table", 1);

	/* Test with second table */
	for (i = 0; i < num_fds; i++) {
		snprintf(tmp, sizeof(tmp), "field-tab2-%d", i + 1);
		tdi[i].name = strdup(tmp);
	}
	idb_table_insert("table-2", num_fds, tdi);

	for (i = 0; i < num_fds; i++) {
		snprintf(tmp, sizeof(tmp), "field-tab2-%d", i + 1);
		tdi[i].sValue = strdup(tmp);
		tdi[i].cData = strdup(tmp);
		tdi[i].cData_len = strlen(tmp);
	}
	idb_table_insert("table-2", num_fds, tdi);
	
	idb_dump();

	idb_save("cdvdata/db/test.cdb");

	sel_fields = (char **)malloc( 2 * sizeof(char *));
	sel_fields[0] = (char *)malloc( 50 * sizeof(char));
	sel_fields[1] = (char *)malloc( 50 * sizeof(char));
	strcpy(sel_fields[0], "field-tab2-1");
	strcpy(sel_fields[1], "field-tab2-2");

	tds = idb_table_select("table-2", 2, sel_fields, 0, NULL);

	idb_results_dump( tds );

	DPRINTF("%s: Applying condition (field-tab2-1 == test)\n", __FUNCTION__);

	where_fields = (tTableDataInput *)malloc( sizeof(tTableDataInput) );
	where_fields[0].name = strdup("field-tab2-1");
	where_fields[0].sValue = strdup("test");

	tds = idb_table_select("table-2", 2, sel_fields, 1, where_fields);
	idb_free();

	idb_load("cdvdata/db/test.cdb");
	idb_dump();

	idb_results_dump( tds );

	free(where_fields);
	free(sel_fields[0]);
	free(sel_fields[1]);
	free(sel_fields);

	return 0;
}

int main(void)
{
	int i;
	char cmd[1024] = { 0 };

	if (ensure_directory_existence(DATADIR) != 0)
		return 1;

	snprintf(cmd, sizeof(cmd), "SET DATADIR %s", DATADIR);
	idb_process_query(cmd);
	idb_process_query("INIT test-data.cdb");
	idb_process_query("CREATE TABLE users(id int, username string, password string, hash string) COMMENT 'Comment';");
	idb_process_query("CREATE TABLE users2(id int, username string, password string, hash string);");
	//idb_process_query("INSERT INTO users(id, username, password, hash) VALUES( 1, 'username', 'password', 'hash' );");
	idb_process_query("UPDATE users SET username = 'un', hash = 'hh' WHERE id = 1");
	//idb_process_query("COMMIT");
	idb_process_query("ROLLBACK");
	idb_process_query("DUMP");
	idb_process_query("CLOSE");
	return 0;

	//i = test_xml_query_data();
	i = test_idb();

	//i = load_project("./examples/test/test.project");
	return i;
}

