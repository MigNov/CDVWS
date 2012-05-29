#define DEBUG_SHELL
#include "cdvws.h"

#ifdef DEBUG_SHELL
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/cdv-shell   ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

#ifdef USE_READLINE
char *get_string(char *prompt)
{
	char *tmp = readline(prompt);

	add_history(tmp);
	return tmp;
}
#else
char *get_string(char *prompt)
{
	int c;
	char a[2] = { 0 };
	char buf[1024] = { 0 };

	printf("%s", prompt);
	while ((c = getchar()) != '\n') {
		a[0] = c;
		strcat(buf, a);
	}

	return strdup( buf );
}
#endif

int idb_shell(void)
{
	float fTm = -1;
	struct timespec ts = utils_get_time( TIME_CURRENT );
	struct timespec tse;

	printf("\nCDV WebServer v%s internal database (iDB) shell\n", VERSION);
	while (1) {
		char *str = get_string("idb> ");

		if (str == NULL)
			return -EIO;

		if (((strlen(str) > 0) && (str[0] == -1))
			|| (strcmp(str, "\\q") == 0)
			|| (strcmp(str, "quit") == 0))
				break;

		if (strcmp(str, "help") == 0) {
			printf("iDB shell help:\n\nNote: Commands *are* case sensitive\n\n"
				"SET MINCRYPT <password> <salt>\t- set minCrypt encryption if available\n"
				"SET FRESHQUERYLOG <filename>\t- set query logging with log overwriting\n"
				"SET QUERYLOG <filename>\t\t- set query logging without log overwriting\n"
				"SET DATADIR <directory>\t\t- set data directory\n"
				"SET FILENAME <filename>\t\t- set filename to write database to\n"
				"REINIT <filename>\t\t- initialize a fresh database in <filename>\n"
				"INIT <filename>\t\t\t- initialize a database in <filename>\n"
				"CLOSE\t\t\t\t- close database session\n\n"
				"Database manipulation commands:\n\n"
				"CREATE TABLE\t\t- create a table in database\n"
				"\t\t\t\tExample: CREATE TABLE table(id int, data string) COMMENT 'Comment';\n"
				"SELECT\t\t\t- select data from database table\n"
				"\t\t\t\tExample: SELECT * FROM table WHERE id = 1\n"
				"INSERT\t\t\t- insert a row into database table\n"
				"\t\t\t\tExample: INSERT INTO table(id, data) VALUES( 1, 'data' );\n"
				"UPDATE\t\t\t- update row in database table\n"
				"\t\t\t\tExample: UPDATE table SET data = 'new-data'[, id = 1] WHERE id = 1\n"
				"DELETE\t\t\t- delete row from database table\n"
				"\t\t\t\tExample: DELETE FROM table WHERE id = 1\n"
				"DROP TABLE\t\t- drop table in a database\n"
				"\t\t\t\tExample: DROP TABLE table\n"
				"COMMIT\t\t\t- commit changes to database file\n"
				"ROLLBACK\t\t- rollback changes\n"
				"DUMP\t\t\t- dump all data\n\n"
				"Other commands:\n\n"
				"pwd\t\t\t- print current working directory\n"
				"quit\t\t\t- end IDB session\n"
				"\n");
		}
		else
		if (strcmp(str, "pwd") == 0) {
			char buf[1024] = { 0 };

			getcwd(buf, sizeof(buf));
			puts(buf);
		}
		else
		if (strlen(str) > 0) {
			int ret = idb_query(str);
			if ((strcmp(str, "COMMIT") == 0) && (ret == -EINVAL)) {
				/* File name is missing, ask for new file name */
				char *tmp = get_string("File name is not set. Please enter name of file to "
						"write database to.\nFile name: ");
				char tmp2[4096] = { 0 };

				snprintf(tmp2, sizeof(tmp2), "SET FILENAME %s", tmp);
				free(tmp);

				idb_query(tmp2);
				ret = idb_query(str);
			}

			if (strncmp(str, "SELECT", 6) == 0) {
				idb_results_dump( idb_get_last_select_data() );
				idb_free_last_select_data();
			}

			printf("Query '%s' returned with error code %d\n", str, ret);
		}
	}

	idb_free();
	printf("\n");

	tse = utils_get_time( TIME_CURRENT );
	fTm = get_time_float_us( tse, ts );
	DPRINTF("%s: IDB Shell session time is %.3f s\n", __FUNCTION__,
		fTm / 1000000);

	return 0;
}

int run_shell(void)
{
	float fTm = -1;
	struct timespec ts = utils_get_time( TIME_CURRENT );
	struct timespec tse;

	first_initialize();

	printf("\nCDV WebServer v%s shell\n", VERSION);
	while (1) {
		char *str = get_string("(cdv) ");

		if (str == NULL)
			return -EIO;

		if (((strlen(str) > 0) && (str[0] == -1))
			|| (strcmp(str, "\\q") == 0)
			|| (strcmp(str, "quit") == 0))
				break;

		if (strncmp(str, "load_project ", 13) == 0) {
			if (access(str + 13, R_OK) != 0) {
				printf("Error: Cannot read project file '%s'\n", str + 13);
			}
			else {
				int ret = load_project( str + 13 );

				if (ret == 0) {
					DPRINTF("%s: Project file '%s' has been loaded successfully\n",
						__FUNCTION__, str + 13);
					_shell_project_loaded = 1;
					printf("Project file '%s' loaded successfully\n", str + 13);
				}
				else
					printf("Error: Project file '%s' load failed with error code %d\n",
						str + 13, ret);
			}
		}
		else
		if (strncmp(str, "dump", 4) == 0) {
			tTokenizer t = tokenize(str, " ");

			if (dump_file_is_set())
				printf("Using dump log file for writing these data\n");

			if (t.numTokens == 1) {
				project_dump();
			}
			else {
				if (strcmp(t.tokens[1], "info") == 0)
					project_info_dump();
				else
				if (strcmp(t.tokens[1], "config") == 0)
					config_variable_dump(
						(t.numTokens == 3) ? t.tokens[2] : NULL);
				else
				if (strcmp(t.tokens[1], "help") == 0)
					printf("Dump command help:\n\ndump - dump both info and configuration\n"
						"dump info - dump project information only\n"
						"dump config - dump project configuration only\n\n");
				else
					printf("Unknown option for dump\n");
			}
			free_tokens(t);
		}
		else
		if (strncmp(str, "set", 3) == 0) {
			tTokenizer t = tokenize(str, " ");

			if (t.numTokens < 3) {
				printf("Error: Invalid SET syntax\n");
				continue;
			}

			if (strcmp(t.tokens[1], "dumplog") == 0) {
				if (strcmp(t.tokens[2], "-") == 0) {
					dump_unset_file();
					printf("Dump file unset, will write to stdout\n");
				}
				else {
					if (dump_set_file(t.tokens[2]) != 0)
						printf("Error: Cannot set dump log file to '%s'\n", t.tokens[2]);
					else
						printf("Dump file set to %s\n", t.tokens[2]);
				}
			}

			free_tokens(t);
		}
		else
		if (strcmp(str, "version") == 0) {
			printf("CDV WebServer version: %s\n\n", VERSION);
			printf("No modules found\n");
		}
		else
		if (strcmp(str, "help") == 0) {
			printf("CDV shell help:\n\nload_project <filename>\t- load project file <filename> into memory\n"
				"dump [<space>]\t\t- dump configuration data, see \"dump help\" for more information\n"
				"set dumplog <filename>\t- set file for dump data (or '-' to write back to stdout)\n"
				"version\t\t\t- get version information\n"
				"pwd\t\t\t- print current working directory\n"
				"idbshell\t\t- go into IDB shell\n"
				"\n");
		}
		else
		if (strcmp(str, "pwd") == 0) {
			char buf[1024] = { 0 };

			getcwd(buf, sizeof(buf));
			puts(buf);
		}
		else
		if (strcmp(str, "idbshell") == 0)
			idb_shell();
		else
		if (strlen(str) > 0)
			printf("Error: Unknown command '%s'\n", str);
	}

	if (_shell_project_loaded == 1) {
		DPRINTF("%s: Project loaded, cleaning up\n", __FUNCTION__);
		total_cleanup();
	}

	tse = utils_get_time( TIME_CURRENT );
	fTm = get_time_float_us( tse, ts );
	DPRINTF("%s: Shell session time is %.3f s\n", __FUNCTION__,
		fTm / 1000000);

	return 0;
}
