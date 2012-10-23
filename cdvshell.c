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

	if (strcmp(tmp, "quit") != 0)
		add_history(tmp);

	return trim(tmp);
}

void readline_unlink(char *filename)
{
	char *fn = NULL;

	clear_history();
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

	unlink(fn);
	free(fn);
}

#else
void readline_init(char *filename) { };
void readline_close(void) { };
void readline_set_max(int max) { };
void readline_unlink(char *filename) { };

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

int process_idb_command(struct timespec ts, BIO *io, int cfd, char *str)
{
	int ret;
	float fTm = -1;
	struct timespec tse;
	char *autocommit_fn = NULL;

	if (str == NULL)
		return -EIO;

	/* For the remote shell, we need to use CDV Cookie for data consistency */
	if (SHELL_IS_REMOTE(io, cfd)) {
		if (_cdv_cookie == NULL)
			desc_printf(io, cfd, "I/O Error on CDV Cookie acquisition!\n");
		else {
			char fn[512] = { 0 };

			snprintf(fn, sizeof(fn), "/tmp/cdvdb-%s", _cdv_cookie);
			autocommit_fn = strdup(fn);

			DPRINTF("%s: Setting up autocommit file name to %s\n", __FUNCTION__, autocommit_fn);

			if (access(autocommit_fn, R_OK) == 0) {
				char tmp[4096] = { 0 };
				int ret;

				snprintf(tmp, sizeof(tmp), "INIT %s", autocommit_fn);
				ret = idb_query(tmp);

				if (ret != 0)
					desc_printf(io, cfd, "Data consistency error!\n");
			}
		}
	}

	if (((strlen(str) > 0) && (str[0] == -1))
		|| (strcmp(str, "\\q") == 0)
		|| (strcmp(str, "quit") == 0))
			return 1;

	if (strcmp(str, "help") == 0) {
		desc_printf(io, cfd, "iDB shell help:\n\nNote: Commands *are* case sensitive\n\n"
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
		write_common(io, cfd, buf, strlen(buf));
		write_common(io, cfd, "\n", 1);
	}
	else
	if (strcmp(str, "time") == 0) {
		char tmp[1024] = { 0 };

		tse = utils_get_time( TIME_CURRENT );
		fTm = get_time_float_us( tse, ts );

		strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&tse.tv_sec));
		if (SHELL_IS_REMOTE(io, cfd))
			desc_printf(io, cfd, "Current server date/time is %s.\n", tmp);
		else
			desc_printf(io, cfd, "Current date/time is %s, current session time is %.3f s (%.3f min, %.3f h)\n",
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
							desc_printf(io, cfd, "Query '%s' returned with code %d\n",
								buf, ret);
						}
					}

					desc_printf(io, cfd, "%d queries processed\n", num);

					fclose(fp);
				}
				else
					desc_printf(io, cfd, "Error: Cannot open file %s for reading\n",
						t.tokens[1]);
			}
			else
				desc_printf(io, cfd, "Error: Cannot access file %s\n", t.tokens[1]);
		}
		else
			desc_printf(io, cfd, "Syntax:\t1. run <filename>\n\t2. \\.  <filename>\n");

		free_tokens(t);
	}
	else
	if (strlen(str) > 0) {
		while (strstr(str, "%20") != NULL)
			str = replace(str, "%20", " ");
		while (strstr(str, "%2C") != NULL)
			str = replace(str, "%2C", ",");
		while (strstr(str, "%3B") != NULL)
			str = replace(str, "%3B", ";");

		ret = idb_query(str);
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
			idb_results_show( io, cfd, idb_get_last_select_data() );
			idb_free_last_select_data();
		}

		desc_printf(io, cfd, "Query '%s' returned with error code %d\n", str, ret);

		if ((autocommit_fn != NULL) && (ret == 0)) {
			char tmp2[4096] = { 0 };

			snprintf(tmp2, sizeof(tmp2), "SET FILENAME %s", autocommit_fn);
			idb_query(tmp2);

			ret = idb_query("COMMIT");
		}
	}

	return ret;
}

int idb_shell(BIO *io, int cfd)
{
	int ret;
	float fTm = -1;
	struct timespec ts = utils_get_time( TIME_CURRENT );
	struct timespec tse;

	if (cfd == STDIN)
		readline_init(READLINE_HISTORY_FILE_IDB);

	desc_printf(io, cfd, "\nCDV WebServer v%s internal database (iDB) shell\n", VERSION);
	while (1) {
		char *str = NULL;

		if ((cfd == STDIN) && (io == NULL))
			str = readline_read("idb> ");
		else
			str = desc_read(io, cfd);

		ret = process_idb_command(ts, io, cfd, str);
		if (ret == 1)
			break;
	}

	idb_free();
	desc_printf(io, cfd, "\n");

	tse = utils_get_time( TIME_CURRENT );
	fTm = get_time_float_us( tse, ts );
	DPRINTF("%s: IDB Shell session time is %.3f s (%.3f min, %.3f h)\n", __FUNCTION__,
		fTm / 1000000, fTm / 1000000 / 60., fTm / 1000000. / 3600.);

	readline_close();
	return ret;
}

int process_shell_command(struct timespec ts, BIO *io, int cfd, char *str, char *ua, char *host)
{
	int ret = 0;
	float fTm = -1;
	struct timespec tse;

	if (str == NULL)
		return -EIO;

	if (((strlen(str) > 0) && (str[0] == -1))
		|| (strcmp(str, "\\q") == 0)
		|| (strcmp(str, "quit") == 0))
			return 1;

	if (strncmp(str, "load", 4) == 0) {
		tTokenizer t = tokenize(str, " ");

		if (t.numTokens < 3) {
			if ((t.numTokens == 2) && (strcmp(t.tokens[1], "help") == 0))
				desc_printf(io, cfd, "Load command help:\n\n"
					"load xml <filename>\t- load XML file <filename> into memory\n"
					"load project <filename>\t- load project file <filename> into memory\n"
					"\n");
			else
				desc_printf(io, cfd, "Syntax: load <type> <filename>\n");
		}
		else {
			if (access(t.tokens[2], R_OK) != 0)
				desc_printf(io, cfd, "Error: Cannot open file '%s'\n", t.tokens[2]);
			else {
				if (strcmp(t.tokens[1], "xml") == 0) {
					int ret = xml_load(t.tokens[2]);

					if (ret == 0)
						desc_printf(io, cfd, "XML file %s loaded successfully\n", t.tokens[2]);
					else
						desc_printf(io, cfd, "Error: Cannot load XML file %s\n", t.tokens[2]);
				}
				else
				if (strcmp(t.tokens[1], "project") == 0) {
					int ret = load_project(t.tokens[2]);

					if (ret == 0) {
						DPRINTF("%s: Project file '%s' has been loaded successfully\n",
							__FUNCTION__, t.tokens[2]);
						_shell_project_loaded = 1;
						desc_printf(io, cfd, "Project file '%s' loaded successfully\n", t.tokens[2]);
					}
					else
						desc_printf(io, cfd, "Error: Project file '%s' load failed with error code %d\n",
							t.tokens[2], ret);
				}
				else
					desc_printf(io, cfd, "Error: Invalid load type\n");
			}
		}

		free_tokens(t);
	}
	else
	if ((strncmp(str, "dump", 4) == 0) && SHELL_IS_REMOTE(io, cfd)) {
		desc_printf(io, cfd, "Remote shell cannot handle dump data\n");
	}
	else
	if ((strncmp(str, "dump", 4) == 0) && !SHELL_IS_REMOTE(io, cfd)) {
		tTokenizer t = tokenize(str, " ");

		if (dump_file_is_set())
			desc_printf(io, cfd, "Using dump log file for writing these data\n");

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
			if (strcmp(t.tokens[1], "xml") == 0)
				xml_dump();
			else
			if (strcmp(t.tokens[1], "vars") == 0)
				variable_dump();
			else
			if (strcmp(t.tokens[1], "help") == 0)
				desc_printf(io, cfd, "Dump command help:\n\n"
					"dump\t\t- dump both info and configuration\n"
					"dump xml\t- dump XML information\n"
					"dump info\t- dump project information only\n"
					"dump vars\t- dump variables for the project\n"
					"dump config\t- dump project configuration only\n\n"
					"Internal shell options:\n\n"
					"dump pids\t- dump server PIDs\n\n");
			else
				desc_printf(io, cfd, "Unknown option for dump\n");
		}
		free_tokens(t);
	}
	else
	if (strncmp(str, "set", 3) == 0) {
		tTokenizer t = tokenize(str, " ");

		if (t.numTokens < 3) {
			desc_printf(io, cfd, "Error: Invalid SET syntax\n");
			return 0;
		}

		if (strcmp(t.tokens[1], "dumplog") == 0) {
			if (strcmp(t.tokens[2], "-") == 0) {
				dump_unset_file();
				desc_printf(io, cfd, "Dump file unset, will write to stdout\n");
			}
			else {
				if (dump_set_file(t.tokens[2]) != 0)
					desc_printf(io, cfd, "Error: Cannot set dump log file to '%s'\n", t.tokens[2]);
				else
					desc_printf(io, cfd, "Dump file set to %s\n", t.tokens[2]);
			}
		}
		else
		if (strcmp(t.tokens[1], "history-limit") == 0) {
			if (t.numTokens == 3) {
				int limit = atoi(t.tokens[2]);
				if (limit > 0) {
					readline_set_max( limit );
					desc_printf(io, cfd, "History limit set to %d\n", limit);
				}
				else
					desc_printf(io, cfd, "Syntax: set history-limit <max>\n");
			}
		}

		free_tokens(t);
	}
	else
	if (strcmp(str, "version") == 0) {
		desc_printf(io, cfd, "CDV WebServer version: %s\n\n", VERSION);
		desc_printf(io, cfd, "No modules found\n");
	}
	else
	if (strcmp(str, "help") == 0) {
		desc_printf(io, cfd, "CDV shell help:\n\n"
			"idbshell\t\t\t\t\t\t- go into IDB shell\n"
			"\n"
			"Settings:\n\n"
			"set dumplog <filename>\t\t\t\t\t- set file for dump data (or '-' to write back to stdout)\n"
			"set history-limit <max>\t\t\t\t\t- set history limit to <max> entries\n"
			"clear history\t\t\t\t\t\t- clear history and history files\n"
			"emulate <method> <data>\t\t\t\t\t- emulate GET or POST method data entry\n"
			"eval <line>\t\t\t\t\t\t- evaluate the script line\n"
			"\n"
			"Testing functions:\n\n"
			"run <type> <params>\t\t\t\t\t- run <type> on shell, see \"run help\" for more information\n"
			"stop\t\t\t\t\t\t\t- stop HTTP and HTTPS servers\n"
			"mime <filename>\t\t\t\t\t\t- get mime type for <filename>\n\n"
			"Debugging/inspection functions:\n\n"
			"p <object>\t\t\t\t\t\t- print object information\n"
			"print <object>\t\t\t\t\t\t- another way to print object information\n"
			"dump [<space>]\t\t\t\t\t\t- dump configuration data, see \"dump help\" for more information\n"
			"load <type> <filename>\t\t\t\t\t- load file into memory, see \"load help\" for more information\n\n"
			"Other functions:\n\n"
			"pwd\t\t\t\t\t\t\t- print current working directory\n"
			"ls <dir>\t\t\t\t\t\t- list files and directories in <dir>\n"
			"time\t\t\t\t\t\t\t- get current and session time\n"
			"info\t\t\t\t\t\t\t- get session information\n"
			"version\t\t\t\t\t\t\t- get version information\n"
			"quit\t\t\t\t\t\t\t- quit shell session\n"
			"\n");
	}
	else
	if (strcmp(str, "clear history") == 0) {
		readline_unlink(READLINE_HISTORY_FILE_CDV);
		readline_unlink(READLINE_HISTORY_FILE_IDB);

		desc_printf(io, cfd, "History and history files cleared\n");
	}
	else
	if (strncmp(str, "eval ", 5) == 0) {
		int ec = 0;

		ec = script_process_line(str + 5);
		desc_printf(io, cfd, "Expression evaluation returned error code %d (%s)\n", ec, strerror(-ec));
	}
	else
	if (strncmp(str, "emulate", 7) == 0) {
		int type = -1;
		tTokenizer t = tokenize(str, " ");

		if (t.numTokens != 3) {
			desc_printf(io, cfd, "Syntax: emulate <method> <data>\nMethod can be either GET or POST and data"
				"can be any valid GET/POST data, i.e. without space\n");
		}
		else {
			if ((strcmp(t.tokens[1], "get") == 0) || (strcmp(t.tokens[1], "GET") == 0))
				type = TYPE_QGET;
			else
			if ((strcmp(t.tokens[1], "post") == 0) || (strcmp(t.tokens[1], "POST") == 0))
				type = TYPE_QPOST;

			if (type == -1)
				desc_printf(io, cfd, "Type is unknown, allowed values are just GET and POST\n");
			else {
				http_parse_data(t.tokens[2], type);
				desc_printf(io, cfd, "Emulation done\n");
			}
		}

		free_tokens(t);
	}
	else
	if (strncmp(str, "run", 3) == 0) {
		tTokenizer t = tokenize(str, " ");

		if (t.numTokens < 2)
			desc_printf(io, cfd, "Syntax: run <type> <params>\n");
		else
		if (strcmp(t.tokens[1], "help") == 0)
			desc_printf(io, cfd, "Run command options:\n\n"
				"run server <http|https> <port> [<private-key> <public-key> <root-key>]"
				"\t- Run HTTP/HTTPS server on specified port\n"
				"run script <filename>\t\t\t\t\t\t\t- Run/process a script specified by <filename>\n\n");
		else
		if (strcmp(t.tokens[1], "script") == 0) {
			tTokenizer t = tokenize(str, " ");

			if (t.numTokens == 3) {
				int ret;

				ret = run_script(t.tokens[2]);
				desc_printf(io, cfd, "Script %s processing finished with error code %d\n", t.tokens[2], ret);
			}
			else
				desc_printf(io, cfd, "Syntax: run script <filename>\n");

			free_tokens(t);
		}
		else
		if (strcmp(t.tokens[1], "server") == 0) {
			if (t.numTokens < 4)
				desc_printf(io, cfd, "Syntax: run server <http|https> <port> [<params>]\n");
			else {
				if (strcmp(t.tokens[2], "http") == 0) {
					int fd[2];
					char buf[128] = { 0 };

					pipe(fd);
					if (fork() == 0) {
						int port;

						snprintf(buf, sizeof(buf), "%d", (int)getpid());
						close(fd[0]);
						write(fd[1], buf, strlen(buf));
						port = atoi(t.tokens[3]);
						free_tokens(t);
						if (run_server( port, NULL, NULL, NULL) != 0)
							desc_printf(io, cfd, "Error: Cannot run server on port %s\n",
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
				else
				if (strcmp(t.tokens[2], "https") == 0) {
					if (t.numTokens < 7)
						desc_printf(io, cfd, "Syntax: run server <http|https> <port> <private-key>"
							"<public-key> <root-key>\n");
					else {
						int fd[2];
						char buf[128] = { 0 };

						pipe(fd);
						if (fork() == 0) {
							int port;
							char *s1 = NULL;
							char *s2 = NULL;
							char *s3 = NULL;

							snprintf(buf, sizeof(buf), "%d", (int)getpid());
							close(fd[0]);
							write(fd[1], buf, strlen(buf));
							port = atoi(t.tokens[3]);
							s1 = strdup(t.tokens[4]);
							s2 = strdup(t.tokens[5]);
							s3 = strdup(t.tokens[6]);
							free_tokens(t);

							if (run_server( port, s1, s2, s3 ) != 0)
								desc_printf(io, cfd, "Error: Cannot run SSL server on port %s\n",
									t.tokens[1]);

							write(fd[1], "ERR", 3);
							free(s1);
							free(s2);
							free(s3);

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
				}
				else
					desc_printf(io, cfd, "Syntax: run server <http|https> <port> <params>\n");
			}
		}
		else
			desc_printf(io, cfd, "Run command options:\n\n"
				"run server <http|https> <port> [<private-key> <public-key> <root-key>]"
				"\t-Run HTTP/HTTPS server on specified port\n\n");

		free_tokens(t);
	}
	else
	if (strcmp(str, "stop") == 0) {
		int ret;

		ret = utils_pid_signal_all(SIGUSR1);
		desc_printf(io, cfd, "Signal sent to %d process(es). Waiting for termination\n",
			ret);
		ret = utils_pid_wait_all();
		desc_printf(io, cfd, "%d process(es) terminated\n", ret);
	}
	else
	if (strcmp(str, "pwd") == 0) {
		char buf[1024] = { 0 };

		getcwd(buf, sizeof(buf));
		write_common(io, cfd, buf, strlen(buf));
		write_common(io, cfd, "\n", 1);
	}
	else
	if (strncmp(str, "ls ", 3) == 0) {
		DIR *d = NULL;
		int num = 0;
		char *dir = NULL;
		struct dirent *entry;

		dir = replace(str + 3, "%2F", "/");
		dir = replace(dir, "%20", " ");
		if (strcmp(dir, "~") == 0)
			write_common(io, cfd, "Home directory disabled\n", 24);
		else {
			d = opendir(dir);
			if (d == NULL)
				desc_printf(io, cfd, "Cannot open directory '%s'\n", dir);
			else {
				while ((entry = readdir(d)) != NULL) {
					if (strlen(entry->d_name) > 0) {
						write_common(io, cfd, entry->d_name,
							strlen(entry->d_name));
						write_common(io, cfd, "   ", 3);

						num++;
					}
				}
				closedir(d);

				if (num == 0)
					write_common(io, cfd, "No entry found", 15);

				write_common(io, cfd, "\n", 1);
			}
		}
	}
	else
	if (strncmp(str, "cat ", 4) == 0) {
		int fd;
		char *path = NULL;

		path = replace(str + 4, "%2F", "/");
		path = replace(path, "%20", " ");

		fd = open(path, O_RDONLY);
		if (fd <= 0)
			desc_printf(io, cfd, "Cannot open file %s", path);
		else {
			int len;
			char buf[BUFSIZE] = { 0 };

			while ((len = read(fd, buf, sizeof(buf))) > 0)
				write_common(io, cfd, buf, len);

			write_common(io, cfd, "\n", 1);
			close(fd);
		}
	}
	else
	if (strcmp(str, "info") == 0) {
		desc_printf(io, cfd, "Shell type is %s%s. Running under user %s (UID %d)",
			SHELL_IS_REMOTE(io, cfd) ? "remote" : "local",
			SHELL_OVER_SSL(io, cfd) ? " over SSL" : "",
			getlogin(), getuid() );

		if ((ua != NULL) && (host != NULL))
			desc_printf(io, cfd, ". User agent is %s for host %s",
				ua, host);

		desc_printf(io, cfd, "\n");
	}
	else
	if (strcmp(str, "idbshell") == 0) {
		idb_shell(io, cfd);

		readline_init(READLINE_HISTORY_FILE_CDV);
	}
	else
	if (strcmp(str, "time") == 0) {
		char tmp[1024] = { 0 };

		tse = utils_get_time( TIME_CURRENT );
		fTm = get_time_float_us( tse, ts );

		strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&tse.tv_sec));

		if (SHELL_IS_REMOTE(io, cfd))
			desc_printf(io, cfd, "Current server date/time is %s.\n", tmp);
		else
			desc_printf(io, cfd, "Current date/time is %s, current session time is %.3f s (%.3f min, %.3f h)\n",
				tmp, fTm / 1000000, fTm / 1000000 / 60., fTm / 1000000. / 3600.);
	}
	else
	if (strncmp(str, "mime", 4) == 0) {
		tTokenizer t = tokenize(str, " ");

		if (t.numTokens == 2) {
			char *tmp = NULL;

			if ((tmp = get_mime_type(t.tokens[1])) == NULL)
				desc_printf(io, cfd, "Error: Cannot get mime type for %s\n", t.tokens[1]);
			else
				desc_printf(io, cfd, "Mime type for %s is %s\n", t.tokens[1], tmp);

			free(tmp);
		}
		else
			desc_printf(io, cfd, "Syntax: mime <filename>\n");

		free_tokens(t);
	}
	else
	if ((strncmp(str, "p", 1) == 0) || (strncmp(str, "print", 5) == 0)) {
		tTokenizer t = tokenize(str, " ");
		if (t.numTokens < 2) {
			desc_printf(io, cfd, "Syntax:\tp <object>\n\tprint <object>\n"
				"Example:\tspace.obj (for configuration variables)\n"
				"\t\t//node/node2.name (for XML variables - load XML and dump it using \"dump xml\", then concatenate node with dot and name)\n");
		}
		else {
			tTokenizer t2 = tokenize(t.tokens[1], ".");
			if (t2.numTokens < 2)
				desc_printf(io, cfd, "Error: Invalid object for inspection\n");
			else {
				if (strcmp(t2.tokens[0], "vars") == 0) {
					char *var = variable_get_element_as_string(t2.tokens[1], NULL);

					desc_printf(io, cfd, "%s\n", var ? var : "No value found");

					free(var);
					free(str);
					return 0;
				}

				int i, found = 0;
				char *cfg = NULL;
				char tmp[512] = { 0 };

				for (i = 1; i < t2.numTokens; i++) {
					strcat(tmp, t2.tokens[i]);

					if (i < t2.numTokens - 1)
						strcat(tmp, ".");
				}

				if ((cfg = config_get(t2.tokens[0], tmp)) != NULL) {
					desc_printf(io, cfd, "Configuration variable %s value is: %s\n",
						t.tokens[1], cfg);

					found = 1;
				}

				free(cfg);

				if (found == 0) {
					int num = -1;
					char **ret = xml_get_all(t.tokens[1], &num);

					if ((num > 0) && (ret != NULL)) {
						for (i = 0; i < num; i++)
							desc_printf(io, cfd, "XML variable %s #%d value is: %s\n",
								t.tokens[1], i + 1, ret[i]);
					}

					xml_free_all(ret, num);
					found = (num > 0);
				}

				if (found == 0)
					desc_printf(io, cfd, "No object %s found\n", t.tokens[1]);
			}
			free_tokens(t2);
		}
		free_tokens(t);
	}
	else
	if (strlen(str) > 0)
		desc_printf(io, cfd, "Error: Unknown command '%s'\n", str);

	free(str);

	return ret;
}

int run_shell(BIO *io, int cfd)
{
	int ret = -EINVAL;
	float fTm = -1;
	struct timespec ts = utils_get_time( TIME_CURRENT );
	struct timespec tse;

	first_initialize(1);

	if (cfd == STDIN)
		readline_init(READLINE_HISTORY_FILE_CDV);

	script_set_descriptors(io, cfd);

	desc_printf(io, cfd, "\nCDV WebServer v%s shell\n", VERSION);
	while (1) {
		char *str = NULL;

		if ((cfd == STDIN) && (io == NULL))
			str = readline_read("(cdv) ");
		else
			str = desc_read(io, cfd);

		ret = process_shell_command(ts, io, cfd, str, NULL, NULL);
		if (ret == 1)
			break;
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

	DPRINTF("%s: Shell session time is %.3f s (%.3f min, %.3f h)\n", __FUNCTION__,
		fTm / 1000000, fTm / 1000000 / 60., fTm / 1000000. / 3600.);

	return 0;
}

