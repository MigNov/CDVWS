#define DEBUG_SCRIPTING

#include "cdvws.h"

#ifdef DEBUG_SCRIPTING
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/scripting   ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int _script_builtin_function(char *var, char *fn, char *args)
{
	if (strcmp(fn, "get") == 0) {
		char *val = variable_get_element_as_string(args, "get");

		if (val != NULL) {
			DPRINTF("%s: Processing internal 'get' function for arguments: %s\n", __FUNCTION__, args);
			DPRINTF("%s: Variable %s processed to %s\n", __FUNCTION__, var, val);

			if (var != NULL)
				variable_add(var, val, TYPE_QSCRIPT, -1, gettype(val));
		}
	}
	else
	if (strcmp(fn, "post") == 0) {
		char *val = variable_get_element_as_string(args, "post");

		if (val != NULL) {
			DPRINTF("%s: Processing internal 'post' function for arguments: %s\n", __FUNCTION__, args);
			DPRINTF("%s: Variable %s processed to %s\n", __FUNCTION__, var, val);

			if (var != NULL)
				variable_add(var, val, TYPE_QSCRIPT, -1, gettype(val));
		}
	}
	else
	if (strcmp(fn, "dumptype") == 0) {
		char *str = variable_get_type_string(args, "any");

		desc_printf(gIO, gFd, "Dump type is: %s\n", str ? str : "<null>");
		free(str);
	}
	else
	if (strcmp(fn, "print") == 0) {
		DPRINTF("%s: Value to print is %s\n", __FUNCTION__, args);

		/* TODO: Needs to be fixed! */
		desc_printf(gIO, gFd, "%s", args);
	}
	else
		return -EINVAL;

	return 0;
}

int script_process_line(char *buf)
{
	/* Comparison */
	if (strstr(buf, "==") != NULL) {
		DPRINTF("%s: Comparison found\n", __FUNCTION__);
	}
	else
	/* Assignment */
	if (strstr(buf, "=") != NULL) {
		int i;
		tTokenizer t;

		DPRINTF("%s: Assignment found\n", __FUNCTION__);

		t = tokenize(buf, "=");

		// variable_name = t.tokens[0];
		// variable_value = t.tokens[1];
		char *val = strdup( trim(t.tokens[1]) );
		if (val[strlen(val) - 1] == ';') {
			val[strlen(val) - 1] = 0;

			if (is_numeric(val))
				variable_add(trim(t.tokens[0]), val, TYPE_QSCRIPT, -1, gettype(val));
			else
			if (regex_match("([^(]*)([^)]*)", val)) {
				tTokenizer t2;
				char *args = NULL;
				char *fn;

				t2 = tokenize(val, "(");
				if (t2.tokens[t2.numTokens - 1][strlen(t2.tokens[t2.numTokens - 1]) - 1] != ')')
					return -EINVAL;

				t2.tokens[t2.numTokens - 1][strlen(t2.tokens[t2.numTokens - 1]) - 1] = 0;
				fn = strdup(t2.tokens[0]);

				if (t2.numTokens > 1)
					args = strdup(t2.tokens[1]);
				if (args != NULL) {
					char args2[1024] = { 0 };

					snprintf(args2, sizeof(args2), "%s", args + 1);
					args2[ strlen(args2) - 1] = 0;
					free(args);

					args = strdup( args2 );
				}
				free_tokens(t2);

				if (_script_builtin_function(trim(t.tokens[0]), fn, args) != 0)
					DPRINTF("Function %s doesn't seem to be builtin, we should try user-defined function\n", fn);

				DPRINTF("%s: Should be a function with return value\n", __FUNCTION__);
				free(args);
				free(fn);
			}

			for (i = 0; i < t.numTokens; i++) {
				DPRINTF("%d. %s\n", i + 1, trim(t.tokens[i]));
			}
		}

		free_tokens(t);
	}
	else
	if (regex_match("([^(]*)([^)]*)", buf)) {
		tTokenizer t2;
		char *args = NULL;
		char *fn;

		t2 = tokenize(buf, "(");
		if (t2.tokens[t2.numTokens - 1][strlen(t2.tokens[t2.numTokens - 1]) - 1] != ';')
			return -EINVAL;

		t2.tokens[t2.numTokens - 1][strlen(t2.tokens[t2.numTokens - 1]) - 1] = 0;
		fn = strdup(t2.tokens[0]);

		if (t2.numTokens > 1)
			args = strdup(t2.tokens[1]);

		if (args != NULL) {
			if (args[strlen(args) - 1] == ')')
				args[strlen(args) - 1] = 0;
		}
		free_tokens(t2);

		if (_script_builtin_function(NULL, fn, args) != 0)
			DPRINTF("Function %s doesn't seem to be builtin, we should try user-defined function\n", fn);

		free(args);
		free(fn);
		DPRINTF("%s: Should be a function with ignoring/without return value\n", __FUNCTION__);
	}
	else
		DPRINTF("%s: Not implemented yet\n", __FUNCTION__);

	return -ENOTSUP;
}

int run_script(char *filename)
{
	FILE *fp;
	int opened = 0;
	char buf[4096] = { 0 };

	if (access(filename, R_OK) != 0)
		return -ENOENT;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return -EPERM;

	while (!feof(fp)) {
		memset(buf, 0, sizeof(buf));
		fgets(buf, sizeof(buf), fp);

		if ((strlen(buf) > 1) && (buf[strlen(buf) - 1] == '\n'))
			buf[strlen(buf) - 1] = 0;

		if ((strlen(buf) > 1) && (buf[0] != '\n')) {
			if (strcmp(buf, "<$") == 0)
				opened = 1;
			if (strcmp(buf, "$>") == 0)
				opened = 0;

			if ((opened) && (strcmp(buf, "<$") != 0))
				script_process_line(trim(buf));
		}
	}

	variable_dump();
	fclose(fp);
	return 0;
}

#ifdef USE_SSL
void script_set_descriptors(BIO *io, int fd)
{
	gIO = io;
	gFd = fd;
}
#else
void script_set_descriptors(void *io, int fd)
{
	gIO = NULL;
	gFd = fd;
}
#endif
