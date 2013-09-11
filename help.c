#define DEBUG_HELP

#include "cdvws.h"

#ifdef DEBUG_HELP
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/help        ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

void _help_notifiers(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: notifiers (show|start|stop|files) [index]\n\nWhere:\n"
				"\t\tshow\t- show list of all created notifiers\n"
				"\tstart <index>\t- start notifier with index <index>\n"
				"\t stop <index>\t- stop notifier with index <index>\n"
				"\tfiles <index>\t- show files watched by notifier <index>\n\n"
				"Notifiers are being used to watch for file system changes, usually "
				"for the iDB changes\nto notify users about the change and/or to "
				"trigger database reload. The list of all\nnotifiers shows all "
				"the notifier indexes and the index can be used to start or stop the\n"
				"notifier or to show all the files being watched by the notifier.\n");
}

void _help_idbshell(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: idbshell\n\nIDB Shell is the shell for internal database "
			"(iDB) used as the lightweight database in CDV websites.\n"
			"Database files integrity SHOULD be ensured by read/write locks applied "
			"to the database files however\nopening iDB files on a running website is "
			"not recommended.\n");
}

void _help_set(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: set <variable> <value>\n\nThe 'set' function is used to "
			"set the variable to specified value. Meaning of the value is variable\n"
			"according to the variable being accessed. Possible variables are:\n\n"
			"dumplog <filename>\t- dumplog is used to set file where to save dump data\n"
			"history-limit <max>\t- history limit sets the limit of shell history to <max> entries\n"
			"\n");
}

void _help_clear(BIO *io, int cfd, char *subcmd)
{
	desc_printf(io, cfd, "Syntax: clear <subcommand>\n\nThe 'clear' command clears the specified "
			"entries, like the history and iDB history of this shell,\ndepending on the "
			"subcommand. There's only one valid subcommand so far:\n"
			"\thistory - clears the history of CDV and IDB shell\n"
			"\n");
}

void _help_emulate(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: emulate <method> <data>\n\nEmulate command is emulating the data "
			"entry by standard GET or POST requests.\nThe methods supported are only GET and POST "
			"and the data have to be in HTTP\nGET/POST format to be formatted into correct variables."
			" The script executed\nafter this command will use these values as they were "
			"coming directly from\nuser's web-browser which is very useful for testing.\n\n\n"
			"See also:\n\n\thelp run script\t- help for running the script files\n");
}

void _help_eval(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: eval <line>\n\nThis function is used to evaluate the script line "
			"the same way as it's found in\nthe script. This is useful for functionality testing "
			"of commands written directly\ninto the script file.\n");
}

void _help_kill(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: kill <pid>\n\nThis function allows user of CDV shell to kill the specified "
			"process of CDV WebServer.\nOnly the PIDs that belongs to the CDV WebServer process can be "
			"killed. For security\nreasons other processes or control process cannot be killed.\n");
}

void _help_free(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: free\n\nThis function shows information about total and free shared memory "
			"of CDV WebServer system.\nThe allocation is divided to process IDs information and also "
			"hostings information with the\nmaximum limits, as well as information about used and free "
			"positions in the shared memory.\n");
}

#ifdef USE_GEOIP
void _help_geoip(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: geoip <database-file> <ip>\n\nThe geoip function accepts 2 arguments, first "
			"is the path to the GeoIP database file and the\nIP address as the second argument which can "
			"be either IPv4 or IPv6 address. The information\nare being output according to the information "
			"read from the database file.\n");
}
#endif

void _help_run(BIO *io, int cfd, char *subcmd)
{
	if ((subcmd != NULL) && (strcmp(subcmd, "server") == 0))
		desc_printf(io, cfd, "Syntax: run <http|https> <port> [<private-key> <public-key> <root-cert>]\n\nThis "
			"command allows CDV shell user to run HTTP or HTTPS server on the specified port. For\nHTTP protocol "
			"only port declaration is necessary however for HTTPS the private key, public\nkey and root "
			"certificate are being required to ensure valid SSL connection.\n");
	else
	if ((subcmd != NULL) && (strcmp(subcmd, "script") == 0))
		desc_printf(io, cfd, "Syntax: run script <filename>\n\nThis command is used to run/process the script "
			"specified by the <filename>.\nThe script specified is run the same way as when accessed by the web-browser\n"
			"in the real-life scenario. To emulate GET/POST variables input run 'emulate'\ncommand prior to "
			"running the script using this command.\n\n\nSee also:\n\n\thelp emulate\t- help for emulating GET/POST variables\n");
	else
		desc_printf(io, cfd, "Syntax: run <subcommand> <parameters>\n\nFunction is running the specified subcommand with "
				"parameters.\nOnly 2 subcommands are being supported by now:\n\n"
				"\tserver <params> - run server\n"
				"\tscript <filename> - run a script specified by <filename>\n");
}

void _help_stop(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: stop\n\nFunction is used to stop all the HTTP and HTTPS server being run by the CDV WebServer\n"
			"system. This is useful if you would like to gracefully stop all the servers (killing\nthem is not recommended).\n");
}

void _help_hash(BIO *io, int cfd, char *subcmd)
{
	desc_printf(io, cfd, "Syntax: hash <string> <salt> <len> <flags>\n\nThe 'hash' function is computing the internal hash for <string> with "
			"the specified <salt>.\nThe resulting hash is <len> characters long and consists of alphabetical characters (both\n"
			"uppercase and lowercase) as well as numeric digits. This is not meant to be a security\nfeature rather then "
			"integrity checking feature. The <flags> variable have to be integer,\ne.g. 0. The <flags> is OR'ed 32-bit integer.");

	if ((subcmd == NULL) || (strcmp(subcmd, "flags") != 0)) {
			desc_printf(io, cfd, "\n\nFor more information about flags computations please see 'help hash flags'.\n\n");
			return;
	}

	desc_printf(io, cfd, "\n\nThe <flags> are computed by OR'ing following values:\n\n"
			"\tHASH_FLAGS_HAVE_SEED (value of 1)\t- used to identify we have seed set in <flags> bits\n"
			"\tHASH_FLAGS_PREPEND_SALT (value of 2)\t- used to prepend salt value to hash output string\n"
			"\tHASH_FLAGS_PREPEND_LENGTH (value of 4)\t- used to prepend length value to hash output string\n"
			"\n"
			"This means if you would like to have seed set to 0 (default) you may OR mask HASH_FLAGS_PREPEND_SALT\n"
			"and/or HASH_FLAGS_PREPEND_LENGTH to the <flags>. To set both of them you can simply OR mask them like:\n\n"
			"\tuint32_t flags = HASH_FLAGS_PREPEND_SALT | HASH_FLAGS_PREPEND_LENGTH;\n\n"
			"If you would like to add custom seed in 16-bit range (0 to 65535) to the flags you have to add\nHASH_FLAGS_HAVE_SEED"
			"(value 1) to the flags with your seed value shifted left by 16 bits, e.g. for your\nseed of 22530 you need to OR "
			"expression above by:\n\n"
			"\tflags |= 1 | (22530 << 16)\n\n"
			"OR'ing by 1 (HASH_FLAGS_HAVE_SEED) is necessary to specify we have seed encoded in 16 most significant\nbits of <flags>.\n"
			"The resulting unsigned 32-bit integer value will look like:\n\n"
			"\tflags = ((HASH_FLAGS_PREPEND_SALT | HASH_FLAGS_PREPEND_LENGTH)) | (HASH_FLAGS_HAVE_SEED | (22530 << 16))\n\n"
			"Which means flags will have value of (1 | 2 | 4 | (22530 << 16)) = 0x58020007 (hex) = 1476526087 (dec)\n\n");
}

void _help_crc32(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: crc32 <filename>\n\nThis function computes the CRC-32 checksum value for the file specified by the\n"
			"<filename>. The function returns 0 if the file cannot be opened or is empty.\nThis is being internally used for "
			"checking file integrity when watching for\nfile content changes on the notifiers, if notifiers are compiled in.\n");
}

void _help_mime(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: mime <filename>\n\nThis function gets the MIME type for <filename>. This is useful to check\nwhat "
			"the output MIME type that will be output to the client's browser.\nFunction first checks the entry in /etc/mime.types "
			"file and if no entry\ncould be found then 'file --mime-type -b <filename>' is being executed.\n");
}

void _help_pwd(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: pwd\n\nThis is equivalent of shell's `pwd` function implemented in CDV WebServer.\n");
}

void _help_ls(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: ls <dir>\n\nLists all files and directories in the directory <dir>. Same as shell's `ls` command.\n");
}

void _help_time(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: time\n\nShow current system date and time and also session time.\n");
}

void _help_info(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: info\n\nShow basic information about shell type and user context shell is running under.\n");
}

void _help_version(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: version\n\nShow version information about CDV WebServer system.\n");
}

void _help_quit(BIO *io, int cfd)
{
	desc_printf(io, cfd, "Syntax: quit\n\nQuit the webserver gracefully. Closes all connections established by web server first.\n");
}

void help(BIO *io, int cfd, char *str)
{
	int i;
	char *helpstr = trim(str);

	if (strncmp(helpstr, "help ", 5) != 0)
		return;

	tTokenizer t = tokenize(helpstr, " ");
	if (t.numTokens == 2) {
		if (strcmp(t.tokens[1], "notifiers") == 0)
			_help_notifiers(io, cfd);
		else
		if (strcmp(t.tokens[1], "idbshell") == 0)
			_help_idbshell(io, cfd);
		else
		if (strcmp(t.tokens[1], "set") == 0)
			_help_set(io, cfd);
		else
		if (strcmp(t.tokens[1], "emulate") == 0)
			_help_emulate(io, cfd);
		else
		if (strcmp(t.tokens[1], "eval") == 0)
			_help_eval(io, cfd);
		else
		if (strcmp(t.tokens[1], "kill") == 0)
			_help_kill(io, cfd);
		else
		if (strcmp(t.tokens[1], "free") == 0)
			_help_free(io, cfd);
		#ifdef USE_GEOIP
		else
		if (strcmp(t.tokens[1], "geoip") == 0)
			_help_geoip(io, cfd);
		#endif
		else
		if (strcmp(t.tokens[1], "run") == 0)
			_help_run(io, cfd, NULL);
		else
		if (strcmp(t.tokens[1], "stop") == 0)
			_help_stop(io, cfd);
		else
		if (strcmp(t.tokens[1], "hash") == 0)
			_help_hash(io, cfd, NULL);
		else
		if (strcmp(t.tokens[1], "crc32") == 0)
			_help_crc32(io, cfd);
		else
		if (strcmp(t.tokens[1], "mime") == 0)
			_help_mime(io, cfd);
		else
		if (strcmp(t.tokens[1], "pwd") == 0)
			_help_pwd(io, cfd);
		else
		if (strcmp(t.tokens[1], "ls") == 0)
			_help_ls(io, cfd);
		else
		if (strcmp(t.tokens[1], "time") == 0)
			_help_time(io, cfd);
		else
		if (strcmp(t.tokens[1], "info") == 0)
			_help_info(io, cfd);
		else
		if (strcmp(t.tokens[1], "version") == 0)
			_help_version(io, cfd);
		else
		if (strcmp(t.tokens[1], "quit") == 0)
			_help_quit(io, cfd);
		else
			desc_printf(io, cfd, "Error: Invalid command\n\nSyntax: help <command> "
					"[<subcommand1> ... <subcommandN>]\n");
	}
	else
	if (t.numTokens > 2) {
		if (strncmp(t.tokens[1], "clear", 5) == 0)
			_help_clear(io, cfd, t.tokens[2]);
		else
		if (strncmp(t.tokens[1], "run", 3) == 0)
			_help_run(io, cfd, t.tokens[2]);
		else
		if (strcmp(t.tokens[1], "hash") == 0)
			_help_hash(io, cfd, t.tokens[2]);
		else
			desc_printf(io, cfd, "Error: Invalid subcommand\n\nSyntax: help <command> <subcommand>\n");
	}
	else {
#ifdef DEBUG_HELP
		for (i = 1; i < t.numTokens; i++)
			printf(">>> %d) %s\n", i, t.tokens[i]);
#else
		desc_printf(io, cfd, "Syntax error\n");
#endif
	}

	free_tokens(t);
}

void help_idb(BIO *io, int cfd, char *str)
{
	char *helpstr = trim(str);

	if (strncmp(helpstr, "help ", 5) != 0)
		return;

	tTokenizer t = tokenize(helpstr, " ");
	if (t.numTokens == 2) {
		if (strcmp(t.tokens[1], "INIT") == 0)
			desc_printf(io, cfd, "Syntax: INIT <filename> [RO]\n\nInitializes new iDB database <filename>. "
					"Default mode is read-write, if you want to open in read-only mode put \"RO\" to end.\n");
		else
		if (strcmp(t.tokens[1], "REINIT") == 0)
			desc_printf(io, cfd, "Syntax: REINIT <filename> [RO]\n\nReinitializes a iDB database <filename>. "
					"Default mode is read-write, if you want to open in read-only mode put \"RO\" to end.\n");
		else
		if (strcmp(t.tokens[1], "CLOSE") == 0)
			desc_printf(io, cfd, "Syntax: CLOSE\n\nClose database saving all of the data.\n");
		else
		if ((strcmp(t.tokens[1], "CREATE") == 0) || (strcmp(t.tokens[1], "SELECT") == 0) || (strcmp(t.tokens[1], "INSERT") == 0)
			|| (strcmp(t.tokens[1], "UPDATE") == 0) || (strcmp(t.tokens[1], "DELETE") == 0) || (strcmp(t.tokens[1], "DROP") == 0)
			|| (strcmp(t.tokens[1], "COMMIT") == 0) || (strcmp(t.tokens[1], "ROLLBACK") == 0) || (strcmp(t.tokens[1], "DUMP") == 0))
			desc_printf(io, cfd, "SQL Commands have same meanings like the commands for the standard relational databases.\n");
		if (strcmp(t.tokens[1], "run") == 0)
			desc_printf(io, cfd, "Syntax: run <filename>\n\nRun a SQL command batch from <filename>\n");
		else
		if (strcmp(t.tokens[1], "pwd") == 0)
			desc_printf(io, cfd, "Syntax: pwd\n\nShow current working directory. Same as shell's \"pwd\" command\n");
		else
		if (strcmp(t.tokens[1], "time") == 0)
			desc_printf(io, cfd, "Syntax: time\n\nShow current iDB shell time statistics.\n");
		else
		if (strcmp(t.tokens[1], "quit") == 0)
			desc_printf(io, cfd, "Syntax: quit\n\nQuit iDB shell and close iDB database gracefully. Write is not performed (use COMMIT instead).\n");
		else
			desc_printf(io, cfd, "Error: Invalid command\n\nSyntax: help <command> "
				"[<subcommand1> ... <subcommandN>]\n");
	}
	else
	if (t.numTokens > 2) {
		if (strcmp(t.tokens[1], "SET") == 0) {
			if (strcmp(t.tokens[2], "MINCRYPT") == 0)
				desc_printf(io, cfd, "Syntax: SET MINCRYPT <password> <salt>\n\nSet the minCrypt password to be "
					"used for data encryption/decryption. The salt have to be provided as well.\n");
			else
			if (strcmp(t.tokens[2], "FRESHQUERYLOG") == 0)
				desc_printf(io, cfd, "Syntax: SET FRESHQUERYLOG <filename>\n\nSet fresh query log filename to be "
					"saved on the <filename>. Fresh means it rewrites the log from scratch.\n");
			else
			if (strcmp(t.tokens[2], "QUERYLOG") == 0)
				desc_printf(io, cfd, "Syntax: SET QUERYLOG <filename>\n\nSet query log filename to be "
					"saved on the <filename>.\n");
			else
			if (strcmp(t.tokens[2], "DATADIR") == 0)
				desc_printf(io, cfd, "Syntax: SET DATADIR <directory>\n\nSet the data directory for files\n");
			else
			if (strcmp(t.tokens[2], "FILENAME") == 0)
				desc_printf(io, cfd, "Syntax: SET FILENAME <filename>\n\nSet database file name to write data to.\n");
		}
	}

	free_tokens(t);
}

