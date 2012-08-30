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

	if ((err = idb_table_create("table", num_fds, fd, NULL)) < 0) {
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

	if ((err = idb_table_create("table-2", num_fds, fd, "comment")) < 0) {
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
	idb_table_update_row("table", 2, num_fds, tdi);

	DPRINTF("%s: Now deleting row 1 of table\n", __FUNCTION__);
	idb_table_delete_row("table", 1);

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

int test_idb_queries(void)
{
	char cmd[1024] = { 0 };

	if (ensure_directory_existence(DATADIR) != 0)
		return 1;

	idb_query("SET FRESHQUERYLOG queries.log");

	snprintf(cmd, sizeof(cmd), "SET DATADIR %s", DATADIR);
	idb_query(cmd);
	idb_query("SET MINCRYPT password salt");
	//idb_query("SET MINCRYPT test tester");
	idb_query("REINIT test-data.cdb");
	idb_query("CREATE TABLE users(id int, username string, password string, hash string) COMMENT 'Comment';");
	idb_query("CREATE TABLE users2(id int, username string, password string, hash string);");
	idb_query("INSERT INTO users(id, username, password, hash) VALUES( 1, 'username', 'password', 'hash' );");
	idb_query("INSERT INTO users(id, username, password, hash) VALUES( 2, 'username2', 'password2', 'hash2' );");
	idb_query("INSERT INTO users(id, username, password, hash) VALUES( 3, 'username3', 'password3', 'hash3' );");
	idb_query("INSERT INTO users(id, username, password, hash) VALUES( 4, 'username4', 'password4', 'hash4' );");
	idb_query("INSERT INTO users(id, username, password, hash) VALUES( 5, 'username5', 'password5', 'hash5' );");
	idb_query("UPDATE users SET username = 'username-updated-for-id2', hash = 'hash-updated-for-id2' WHERE id = 2");
	idb_query("UPDATE users SET username = 'username-updated', hash = 'hash-updated' WHERE id = 1 AND username = 'username'");
	idb_query("DUMP");

	//idb_query("DELETE FROM users WHERE id = 1");
	//idb_query("DROP TABLE users");
	idb_query("COMMIT");
	//idb_query("ROLLBACK");

	idb_query("SELECT * FROM users WHERE id = 3");
	//idb_query("SELECT id,username FROM users WHERE id = 3");
	idb_results_dump( idb_get_last_select_data() );
	idb_free_last_select_data();
	idb_results_dump( idb_get_last_select_data() );
	idb_query("CLOSE");
	return 0;
}

void run_servers(int port, int ssl_port)
{
	if ((port > 0) && (fork() == 0))
		run_server(port, NULL, NULL, NULL);

	if ((ssl_port > 0) && (fork() == 0))
		run_server(ssl_port, "certs/server-key.private",
			"certs/server-key.pub", "certs/rootcert.pem");
}

void atex(void)
{
	total_cleanup();
}

int main(int argc, char *argv[])
{
	int i = 1;

	atexit( atex );
	if ((argc > 1) && (strcmp(argv[1], "--shell") == 0)) {
		_shell_enabled = 1;
		return run_shell( NULL, STDIN );
	}

	first_initialize(0);
	if ((argc > 1) && (strcmp(argv[1], "--enable-remote-shell-only") == 0))
		_shell_enabled = 1;

	if ((argc > 1) && (strcmp(argv[1], "--test-xmlrpc") == 0)) {
	        printf("Test #1:\n%s\n", xmlrpc_process("<?xml version=\"1.0\"?><methodCall>"
			"<methodName>namespace.method</methodName><params><param><value><int>41</int></value>"
			"</param><param><value><string>str</string></value></param></params></methodCall>"));

		printf("Test #2:\n%s\n", xmlrpc_process("<?xml version=\"1.0\"?><methodCall>"
			"<methodName>namespace.method2</methodName><params><param><struct><member><name>"
			"lowerBound</name><value><i4>18</i4></value></member><member><name>upperBound</name>"
			"<value><i4>139</i4></value></member></struct></param></params></methodCall>"));

		printf("Test #3:\n%s\n", xmlrpc_process("<?xml version=\"1.0\"?><methodCall>"
			"<methodName>namespace.method3</methodName><params><param><struct><member>"
			"<name>lowerBound</name><value><struct><member><name>member1</name><value>"
			"<int>111</int></value></member><member><name>member2</name><value><string>"
			"string of member2</string></value></member></struct></value></member><member>"
			"<name>upperBound</name><value><i4>139</i4></value></member><member><name>something"
			"</name><value><struct><member><name>member1b</name><value><int>1113</int></value>"
			"</member><member><name>member2b</name><value><string>string of member2bb</string>"
			"</value></member><member><name>added</name><value><dateTime.iso8601>20120830T15:55:55"
			"</dateTime.iso8601></value></member></struct></value></member><member><name>b64</name>"
			"<value><base64>dmFsdWU=</base64></value></member></struct></param></params></methodCall>"));

		printf("Test #4:\n%s\n", xmlrpc_process("<?xml version=\"1.0\"?><methodCall>"
			"<methodName>namespace.method4</methodName><params><param><array><data><value>"
			"<i4>12</i4></value><value><string>Egypt</string></value><value><boolean>0</boolean>"
			"</value><value><i4>-31</i4></value></data></array></param></params></methodCall>"));

		printf("Test #5:\n%s\n", xmlrpc_process("<?xml version='1.0'?><methodCall>"
			"<methodName>test</methodName><params><param><value><struct><member><name>test</name>"
			"<value><int>111</int></value></member></struct></value></param></params></methodCall>"));

		printf("Test #6:\n%s\n", xmlrpc_process("<?xml version='1.0'?>\n<methodCall>\n"
			"<methodName>test</methodName>\n<params>\n<param>\n<value><struct>\n<member>\n<name>test</name>\n"
			"<value><int>111</int></value>\n</member>\n</struct></value>\n</param>\n</params>\n</methodCall>\n"));

		return 1;
	}

	run_servers(2305, 2306);

	//i = test_xml_query_data();
	//i = test_idb();
	//i = test_idb_queries();

	//i = load_project("./examples/test/test.project");
	wait(NULL);
	wait(NULL);
	return i;
}

