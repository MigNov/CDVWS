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

	fd = utils_alloc( "main.test_idb.fd", num_fds * sizeof(tTableFieldDef) );
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

	tdi = utils_alloc( "main.test_idb.tdi", num_fds * sizeof(tTableDataInput) );
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
	fd = utils_free("main.test_idb", fd);

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

	sel_fields = (char **)utils_alloc( "main.test_idb.sel_fields", 2 * sizeof(char *));
	sel_fields[0] = (char *)utils_alloc( "main.test_idb.sel_fields[0]", 50 * sizeof(char));
	sel_fields[1] = (char *)utils_alloc( "main.test_idb.sel_fields[1]", 50 * sizeof(char));
	strcpy(sel_fields[0], "field-tab2-1");
	strcpy(sel_fields[1], "field-tab2-2");

	tds = idb_table_select("table-2", 2, sel_fields, 0, NULL);

	idb_results_dump( tds );

	DPRINTF("%s: Applying condition (field-tab2-1 == test)\n", __FUNCTION__);

	where_fields = (tTableDataInput *)utils_alloc( "main.test_idb.where_fields", sizeof(tTableDataInput) );
	where_fields[0].name = strdup("field-tab2-1");
	where_fields[0].sValue = strdup("test");

	tds = idb_table_select("table-2", 2, sel_fields, 1, where_fields);
	idb_free();

	idb_load("cdvdata/db/test.cdb");
	idb_dump();

	idb_results_dump( tds );

	where_fields = utils_free("main.where_fields", where_fields);
	sel_fields[0] = utils_free("main.sel_fields[0]", sel_fields[0]);
	sel_fields[1] = utils_free("main.sel_fields[1]", sel_fields[1]);
	sel_fields = utils_free("main.sel_fields", sel_fields);

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
		run_server(port, NULL, NULL, NULL, TCP_IPV4 | TCP_IPV6);

	if ((ssl_port > 0) && (fork() == 0))
		run_server(ssl_port, "certs/server-key.private",
			"certs/server-key.pub", "certs/rootcert.pem", TCP_IPV4 | TCP_IPV6);
}

void atex(void)
{
	int i;

	/* Close all descriptors allocated by this process */
	DPRINTF("%s: Number of file descriptors is %d (PID #%d)\n",
		__FUNCTION__, _tcp_sock_fds_num, getpid());
	for (i = 0; i < _tcp_sock_fds_num; i++) {
		DPRINTF("%s: Shutting down fd %d\n", __FUNCTION__,
			_tcp_sock_fds[i].fd);

		shutdown(_tcp_sock_fds[i].fd, SHUT_RDWR);
		close(_tcp_sock_fds[i].fd);
	}

	/* Memory should be freed once all instances are done */
	if (getpid() == parent_pid) {
		utils_pid_wait_all();

		DPRINTF("Freeing shared memory\n");
		shared_mem_free();
	}
	total_cleanup();

	DPRINTF("Process with PID #%d exiting now...\n", getpid());
	utils_pid_delete( getpid() );
	utils_hosting_delete( getpid() );
}

void test_alloc(void)
{
	char *s = utils_alloc( "main.test_alloc", 1024 );

	s = utils_free("main.test_alloc.s", s);
	s = utils_free("main.test_alloc.s", s);
}

void show_info_banner(void)
{
	printf("CDV WebServer v%s (%s minCrypt support, %s PCRE support, %s GNU Readline support, %s Kerberos 5 support over GSS-API, "
		"%s MySQL database support, %s GeoIP support)\n\n", VERSION,
		USE_MINCRYPT ? "with" : "without",
		USE_PCRE ? "with" : "without",
		USE_READLINE ? "with" : "without",
		USE_KERBEROS ? "with" : "without",
		USE_MYSQL ? "with" : "without",
		USE_GEOIP ? "with" : "without");
}

int main(int argc, char *argv[])
{
	int i = 1;
	unsigned long shmsize;

	atexit( atex );

	test_alloc();
	show_info_banner();

	parent_pid = getpid();

	shmsize = calculate_shared_memory_allocation();
	DPRINTF("Allocating %s of shared memory\n", format_size(shmsize));
	if (shared_mem_init_first() < 0) {
		DPRINTF("Error: Cannot initialize shared memory segment\n");
		return 1;
	}

	utils_pid_add( parent_pid, "Control process");

	if ((argc > 1) && (strcmp(argv[1], "--shell") == 0)) {
		_shell_enabled = 1;
		return run_shell( NULL, STDIN );
	}

	first_initialize(0);
	if ((argc > 1) && (strcmp(argv[1], "--enable-remote-shell-only") == 0))
		_shell_enabled = 1;

	if ((argc > 1) && (strcmp(argv[1], "--test-generate-hash") == 0)) {
		int len = 128;
		char str[] = "this is just a test string";
		char strs[] = "test";
		char salt[] = "salt value1";
		char salt2[] = "salt value2";
		char salts[] = "test";
		char saltt[] = "tesu";
		char *hash = generate_hash(str, salt, len);

		printf("Input data: string '%s', salt '%s', length %d bytes in hex format\n => Resulting hash: %s (%d bytes)\n",
			str, salt, len, hash, (int)strlen(hash));
		hash = utils_free("main.hash1", hash);

		hash = generate_hash(str, salt2, len);
		printf("Input data: string '%s', salt '%s', length %d bytes in hex format\n => Resulting hash: %s (%d bytes)\n",
			str, salt2, len, hash, (int)strlen(hash));
		hash = utils_free("main.hash2", hash);

		hash = generate_hash(strs, salts, len);
		printf("Input data: string '%s', salt '%s', length %d bytes in hex format\n => Resulting hash: %s (%d bytes)\n",
			strs, salts, len, hash, (int)strlen(hash));
		hash = utils_free("main.hash3", hash);

		hash = generate_hash(strs, saltt, len);
		printf("Input data: string '%s', salt '%s', length %d bytes in hex format\n => Resulting hash: %s (%d bytes)\n",
			strs, saltt, len, hash, (int)strlen(hash));
		hash = utils_free("main.hash4", hash);
		return 1;
	}

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
	else
	if ((argc > 1) && (strcmp(argv[1], "--test-rewrite-rules") == 0)) {
		char *trans = NULL;
		char str[] = "/article123-test.html";

		regex_parse("./examples/test/rewrite-rules.def");
		regex_dump();

		printf("Regular Expression Match Testing\n");
		printf("\tOriginal:\t%s\n", str);
		trans = regex_format_new_string(str);
		printf("\tTranslation:\t%s\n", trans ? trans : "<null>");
		trans = utils_free("main.main.trans", trans);

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

