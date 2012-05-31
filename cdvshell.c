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
void readline_init(char *filename)
{
	int ret;
	char *fn = NULL;

	using_history();

	if ((filename != NULL) && (filename[0] == '~')) {
		char buf[BUFSIZE] = { 0 };
		char *hd = NULL;

		strcat(buf, filename + 1);
		if ((hd = getenv("HOME")) != NULL) {
			snprintf(buf, sizeof(buf), "%s%s", hd,
				filename + 1);

			fn = strdup( buf );
		}
	}

	if ((filename != NULL) && (fn == NULL))
		fn = strdup( filename );

	if ((history_length > 0)
		&& (_shell_history_file != NULL)) {
		ret = write_history( _shell_history_file );
		DPRINTF("%s: Writing history to %s returned code %d\n",
			__FUNCTION__, _shell_history_file, ret);
	}

	if (filename == NULL) {
		DPRINTF("%s: Freeing history file\n",
			__FUNCTION__);
		free( _shell_history_file );
		_shell_history_file = NULL;
		return;
	}

	clear_history();
	ret = read_history(fn);
	DPRINTF("%s: Reading history from %s returned code %d\n",
		__FUNCTION__, filename, ret);

	_shell_history_file = strdup( fn );
	free(fn);
}

void readline_set_max(int max)
{
	stifle_history(max);
}

void readline_close(void)
{
	readline_init(NULL);
}

char *readline_read(char *prompt)
{
	char *tmp = readline(prompt);

	add_history(tmp);
	return trim(tmp);
}
#else
void readline_init(char *filename) { };
void readline_close(void) { };
void readline_set_max(int max) { };

char *readline_read(char *prompt)
{
	int c;
	char a[2] = { 0 };
	char buf[1024] = { 0 };

	printf("%s", prompt);
	while ((c = getchar()) != '\n') {
		a[0] = c;
		strcat(buf, a);
	}

	return trim(buf);
}
#endif

int idb_shell(void)
{
	float fTm = -1;
	struct timespec ts = utils_get_time( TIME_CURRENT );
	struct timespec tse;

	readline_init(READLINE_HISTORY_FILE_IDB);

	printf("\nCDV WebServer v%s internal database (iDB) shell\n", VERSION);
	while (1) {
		char *str = readline_read("idb> ");

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
				"CREATE TABLE\t\t\t- create a table in database\n"
				"\t\t\t\t\tExample: CREATE TABLE table(id int, data string) COMMENT 'Comment';\n"
				"SELECT\t\t\t\t- select data from database table\n"
				"\t\t\t\t\tExample: SELECT * FROM table WHERE id = 1\n"
				"INSERT\t\t\t\t- insert a row into database table\n"
				"\t\t\t\t\tExample: INSERT INTO table(id, data) VALUES( 1, 'data' );\n"
				"UPDATE\t\t\t\t- update row in database table\n"
				"\t\t\t\t\tExample: UPDATE table SET data = 'new-data'[, id = 1] WHERE id = 1\n"
				"DELETE\t\t\t\t- delete row from database table\n"
				"\t\t\t\t\tExample: DELETE FROM table WHERE id = 1\n"
				"DROP TABLE\t\t\t- drop table in a database\n"
				"\t\t\t\t\tExample: DROP TABLE table\n"
				"COMMIT\t\t\t\t- commit changes to database file\n"
				"ROLLBACK\t\t\t- rollback changes\n"
				"DUMP\t\t\t\t- dump all data\n\n"
				"Other commands:\n\n"
				"run\t\t\t\t- run a batch of commands (in SQL format)\n"
				"pwd\t\t\t\t- print current working directory\n"
				"time\t\t\t\t- get current and IDB session time\n"
				"quit\t\t\t\t- end IDB session\n"
				"\n");
		}
		else
		if (strcmp(str, "pwd") == 0) {
			char buf[1024] = { 0 };

			getcwd(buf, sizeof(buf));
			puts(buf);
		}
		else
		if (strcmp(str, "time") == 0) {
			char tmp[1024] = { 0 };

			tse = utils_get_time( TIME_CURRENT );
			fTm = get_time_float_us( tse, ts );

			strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&tse.tv_sec));
			printf("Current date/time is %s, current session time is %.3f s (%.3f min, %.3f hod)\n",
				tmp, fTm / 1000000, fTm / 1000000 / 60., fTm / 1000000. / 3600.);
		}
		else
		if ((strncmp(str, "run", 3) == 0) || (strncmp(str, "\\.", 2) == 0)) {
			tTokenizer t = tokenize(str, " ");

			if (t.numTokens == 2) {
				if (access(t.tokens[1], R_OK) == 0) {
					FILE *fp = NULL;
					char buf[BUFSIZE];

					fp = fopen(t.tokens[1], "r");
					if (fp != NULL) {
						int num = 0;

						while (!feof(fp)) {
							memset(buf, 0, sizeof(buf));

							fgets(buf, sizeof(buf), fp);
							if ((strlen(buf) > 0)
								&& (buf[strlen(buf) - 1] == '\n'))
								buf[strlen(buf) - 1] = 0;

							if (strlen(buf) > 0) {
								num++;
								int ret = idb_query(buf);
								printf("Query '%s' returned with code %d\n",
									buf, ret);
							}
						}

						printf("%d queries processed\n", num);

						fclose(fp);
					}
					else
						printf("Error: Cannot open file %s for reading\n",
							t.tokens[1]);
				}
				else
					printf("Error: Cannot access file %s\n", t.tokens[1]);
			}
			else
				printf("Syntax:\t1. run <filename>\n\t2. \\.  <filename>\n");

			free_tokens(t);
		}
		else
		if (strlen(str) > 0) {
			int ret = idb_query(str);
			if ((strcmp(str, "COMMIT") == 0) && (ret == -EINVAL)) {
				/* File name is missing, ask for new file name */
				char *tmp = readline_read("File name is not set. Please enter name of file to "
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

		free(str);
	}

	idb_free();
	printf("\n");

	tse = utils_get_time( TIME_CURRENT );
	fTm = get_time_float_us( tse, ts );
	DPRINTF("%s: IDB Shell session time is %.3f s (%.3f min, %.3f hod)\n", __FUNCTION__,
		fTm / 1000000, fTm / 1000000 / 60., fTm / 1000000. / 3600.);

	readline_close();
	return 0;
}

int run_shell(void)
{
	float fTm = -1;
	struct timespec ts = utils_get_time( TIME_CURRENT );
	struct timespec tse;

	first_initialize();

	readline_init(READLINE_HISTORY_FILE_CDV);

	printf("\nCDV WebServer v%s shell\n", VERSION);
	while (1) {
		char *str = readline_read("(cdv) ");

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
				if (strcmp(t.tokens[1], "pids") == 0)
					utils_pid_dump();
				else
				if (strcmp(t.tokens[1], "help") == 0)
					printf("Dump command help:\n\n"
						"dump\t\t- dump both info and configuration\n"
						"dump info\t- dump project information only\n"
						"dump config\t- dump project configuration only\n\n"
						"Internal shell options:\n\n"
						"dump pids\t- dump server PIDs\n\n");
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
			else
			if (strcmp(t.tokens[1], "history-limit") == 0) {
				if (t.numTokens == 3) {
					int limit = atoi(t.tokens[2]);
					if (limit > 0) {
						readline_set_max( limit );
						printf("History limit set to %d\n", limit);
					}
					else
						printf("Syntax: set history-limit <max>\n");
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
			printf("CDV shell help:\n\n"
				"idbshell\t\t\t\t\t\t- go into IDB shell\n"
				"\n"
				"Settings:\n\n"
				"set dumplog <filename>\t\t\t\t\t- set file for dump data (or '-' to write back to stdout)\n"
				"set history-limit <max>\t\t\t\t\t- set history limit to <max> entries\n\n"
				"Testing functions:\n\n"
				"server <port>\t\t\t\t\t\t- start a HTTP server on port <port>\n"
				"ssl-server <port> <private-key> <public-key> <root-key>\t- start a HTTPS server on port <port>\n"
				"kill-servers\t\t\t\t\t\t- kill all HTTP/HTTPS servers\n"
				"get-mime <filename>\t\t\t\t\t- get mime type for <filename>\n\n"
				"Debugging/inspection functions:\n\n"
				"p <object>\t\t\t\t\t\t- print object information\n"
				"print <object>\t\t\t\t\t\t- another way to print object information\n"
				"dump [<space>]\t\t\t\t\t\t- dump configuration data, see \"dump help\" for more information\n"
				"load_project <filename>\t\t\t\t\t- load project file <filename> into memory\n\n"
				"Other functions:\n\n"
				"pwd\t\t\t\t\t\t\t- print current working directory\n"
				"time\t\t\t\t\t\t\t- get current and session time\n"
				"version\t\t\t\t\t\t\t- get version information\n"
				"quit\t\t\t\t\t\t\t- quit shell session\n"
				"\n");
		}
		else
		if ((strncmp(str, "p ", 2) == 0) || (strncmp(str, "print", 5) == 0)) {
			tTokenizer t = tokenize(str, " ");
			if (t.numTokens < 2) {
				printf("Syntax:\tp <object>\n\tprint <object>\n");
			}
			else {
				tTokenizer t2 = tokenize(t.tokens[1], ".");
				if (t2.numTokens < 2)
					printf("Error: Invalid object for inspection\n");
				else {
					int i, found = 0;
					char *cfg = NULL;
					char tmp[512] = { 0 };

					for (i = 1; i < t2.numTokens; i++) {
						strcat(tmp, t2.tokens[i]);

						if (i < t2.numTokens - 1)
							strcat(tmp, ".");
					}

					if ((cfg = config_get(t2.tokens[0], tmp)) != NULL) {
						printf("Configuration variable %s value is: %s\n",
							t.tokens[1], cfg);

						found = 1;
					}

					free(cfg);

					if (found == 0)
						printf("No object %s found\n", t.tokens[1]);
				}
				free_tokens(t2);
			}
			free_tokens(t);
		}
		else
		if (strncmp(str, "server", 6) == 0) {
			tTokenizer t = tokenize(str, " ");
			if (t.numTokens < 2)
				printf("Syntax: server <port>\n");
			else {
				int fd[2];
				char buf[128] = { 0 };

				pipe(fd);
				if (fork() == 0) {
					snprintf(buf, sizeof(buf), "%d", (int)getpid());
					close(fd[0]);
					write(fd[1], buf, strlen(buf));
					if (run_server( atoi(t.tokens[1]), NULL, NULL, NULL) != 0)
						printf("Error: Cannot run server on port %s\n",
							t.tokens[1]);

					write(fd[1], "ERR", 3);

					exit(0);
				}

				/* Try to wait to spawn server, if it fails we will know */
				usleep(50000);

				close(fd[1]);
				read(fd[0], buf, sizeof(buf));
				if (strstr(buf, "ERR") == NULL)
					utils_pid_add( atoi(buf) );
				else
					waitpid( atoi(buf), NULL, 0 );
			}
			free_tokens(t);
		}
		else
		if (strncmp(str, "ssl-server", 10) == 0) {
			tTokenizer t = tokenize(str, " ");
			if (t.numTokens < 5)
				printf("Syntax: ssl-server <port> <private-key> <public-key> <root-key>\n");
			else {
				int fd[2];
				char buf[128] = { 0 };

				pipe(fd);
				if (fork() == 0) {
					snprintf(buf, sizeof(buf), "%d", (int)getpid());
					close(fd[0]);
					write(fd[1], buf, strlen(buf));
					if (run_server( atoi(t.tokens[1]), t.tokens[2], t.tokens[3], t.tokens[4]) != 0)
						printf("Error: Cannot run SSL server on port %s\n",
							t.tokens[1]);

					write(fd[1], "ERR", 3);

					exit(0);
				}

				/* Try to wait to spawn server, if it fails we will know */
				usleep(50000);

				close(fd[1]);
				read(fd[0], buf, sizeof(buf));
				if (strstr(buf, "ERR") == NULL)
					utils_pid_add( atoi(buf) );
				else
					waitpid( atoi(buf), NULL, 0 );
			}
			free_tokens(t);
		}
		else
		if (strcmp(str, "kill-servers") == 0) {
			int ret;

			ret = utils_pid_signal_all(SIGUSR1);
			printf("Signal sent to %d process(es). Waiting for termination\n",
				ret);
			ret = utils_pid_wait_all();
			printf("%d process(es) terminated\n", ret);
		}
		else
		if (strcmp(str, "pwd") == 0) {
			char buf[1024] = { 0 };

			getcwd(buf, sizeof(buf));
			puts(buf);
		}
		else
		if (strcmp(str, "idbshell") == 0) {
			idb_shell();

			readline_init(READLINE_HISTORY_FILE_CDV);
		}
		else
		if (strcmp(str, "time") == 0) {
			char tmp[1024] = { 0 };

			tse = utils_get_time( TIME_CURRENT );
			fTm = get_time_float_us( tse, ts );

			strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&tse.tv_sec));
			printf("Current date/time is %s, current session time is %.3f s (%.3f min, %.3f hod)\n",
				tmp, fTm / 1000000, fTm / 1000000 / 60., fTm / 1000000. / 3600.);
		}
		else
		if (strncmp(str, "get-mime", 8) == 0) {
			tTokenizer t = tokenize(str, " ");

			if (t.numTokens == 2) {
				char *tmp = NULL;

				if ((tmp = get_mime_type(t.tokens[1])) == NULL)
					printf("Error: Cannot get mime type for %s\n", t.tokens[1]);
				else
					printf("Mime type for %s is %s\n", t.tokens[1], tmp);

				free(tmp);
			}
			else
				printf("Syntax: get-mime <filename>\n");

			free_tokens(t);
		}
		else
		if (strlen(str) > 0)
			printf("Error: Unknown command '%s'\n", str);

		free(str);
	}

	if (_shell_project_loaded == 1) {
		DPRINTF("%s: Project loaded, cleaning up\n", __FUNCTION__);
		total_cleanup();
	}

	readline_close();

	tse = utils_get_time( TIME_CURRENT );
	fTm = get_time_float_us( tse, ts );

	utils_pid_signal_all(SIGUSR1);
	utils_pid_wait_all();

	DPRINTF("%s: Shell session time is %.3f s (%.3f min, %.3f hod)\n", __FUNCTION__,
		fTm / 1000000, fTm / 1000000 / 60., fTm / 1000000. / 3600.);

	return 0;
}
