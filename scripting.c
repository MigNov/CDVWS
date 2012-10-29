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
	if (strcmp(fn, "set_all_variables_overwritable") == 0) {
		if (args != NULL) {
			int var = get_boolean(args);

			if ((var == 0) || (var == 1))
				variable_allow_overwrite(NULL, var);
			else
				desc_printf(gIO, gFd, "Invalid value for %s(): %s\n",
					fn, args);
		}
	}
	else
	if (strcmp(fn, "get_all_variables_overwritable") == 0) {
		if (var != NULL) {
			char tmp[4] = { 0 };
			snprintf(tmp, sizeof(tmp), "%d", variable_get_overwrite(NULL));
			variable_add(var, tmp, TYPE_QSCRIPT, -1, TYPE_INT);
		}
		else
			desc_printf(gIO, gFd, "All variables overwritable: %s\n",
				(variable_get_overwrite(NULL) == 1) ? "true" : "false");
	}
	else
	if (strcmp(fn, "set_variable_overwritable") == 0) {
		tTokenizer t;

		t = tokenize(args, ",");
		if (t.numTokens == 2)
			variable_allow_overwrite(trim(t.tokens[0]), get_boolean(t.tokens[1]));
		else
			desc_printf(gIO, gFd, "Syntax: set_variable_overwritable(variable, true|false)\n");

		free_tokens(t);
	}
	else
	if (strcmp(fn, "get_variable_overwritable") == 0) {
		char tmp[4] = { 0 };

		if ((args != NULL) && (strlen(args) > 0))
		{
			snprintf(tmp, sizeof(tmp), "%d", variable_get_overwrite(args));

			if (var != NULL)
				variable_add(var, tmp, TYPE_QSCRIPT, -1, TYPE_INT);
			else
				desc_printf(gIO, gFd, "Variable %s overwritable: %s\n",
					args, (strcmp(tmp, "1") == 0) ? "true" :
					((strcmp(tmp, "0") == 0) ? "false" : "not found"));
		}
		else
			desc_printf(gIO, gFd, "Variable name is missing\n");
	}
	else
	if (strcmp(fn, "del") == 0) {
		variable_set_deleted(args, 1);
	}
	else
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

		desc_printf(gIO, gFd, "%s\n", str ? str : "<null>");
		free(str);
	}
	else
	if (strcmp(fn, "print") == 0) {
		if (args != NULL) {
			if ((args[0] == '"') || (args[0] == '\'')) {
				*args++;
				args[strlen(args) - 1] = 0;

				args = replace(args, "\\n", "\n");
				desc_printf(gIO, gFd, "%s", args);
			}
			else {
				char *var = variable_get_element_as_string(args, NULL);
				desc_printf(gIO, gFd, "%s", var ? var : "");
				free(var);
			}
		}
	}
	else
	if (strcmp(fn, "printf") == 0) {
		if ((args != NULL) && (strlen(args) > 0)) {
			int i;
			tTokenizer t;

			*args++;

			t = tokenize(args, "\"");
			if (t.numTokens == 2) {
				tTokenizer t2;
				char *instr = NULL;
				char *vars = NULL;

				instr = strdup( t.tokens[0] );
				vars = strdup( t.tokens[1] + 1 );

				t2 = tokenize(vars, ",");
				for (i = 0; i < t2.numTokens; i++)
					instr = replace(instr, "%s", variable_get_element_as_string(trim(t2.tokens[i]), NULL));

				while (strstr(instr, "\\n") != NULL)
					instr = replace(instr, "\\n", "\n");

				desc_printf(gIO, gFd, "%s", instr);
				free_tokens(t2);
			}
			else {
				free_tokens(t);
				return -EINVAL;
			}

			free_tokens(t);
		}
		else {
			desc_printf(gIO, gFd, "Invalid syntax for printf()\n");
			return -EINVAL;
		}
	}
	else
		return -EINVAL;

	return 0;
}

int script_process_line(char *buf)
{
	int ret = -ENOTSUP;

	/* Skip if it's a one line comment (more to be implemented) */
	if (is_comment(buf) == 1) {
		DPRINTF("%s: Found comment, skipping\n", __FUNCTION__);
		return 0;
	}

	/* Comparison */
	if (strstr(buf, "==") != NULL) {
		DPRINTF("%s: Comparison found\n", __FUNCTION__);
	}
	else
	/* Definition */
	if (strncmp(buf, "define ", 7) == 0) {
		tTokenizer t;

		t = tokenize(buf + 7, " ");
		if (t.numTokens != 2)
			return -EINVAL;
		if (variable_create(trim(t.tokens[0]), trim(t.tokens[1])) != 1)
			return -EIO;
	}
	else
	/* Operators */
	if ((strstr(buf, "+=") != NULL) || (strstr(buf, "-=") != NULL) ||
		(strstr(buf, "%=") != NULL) || (strstr(buf, "*=") != NULL) ||
		(strstr(buf, "/=") != NULL)) {
		tTokenizer t;
		char *var;
		char *val;
		int op, vtype;

		t = tokenize(buf, "=");
		if (t.numTokens != 2)
			return -EINVAL;

		var = trim(strdup(t.tokens[0]));
		val = trim(strdup(t.tokens[1]));

		op = var[ strlen(var) - 1];
		var[ strlen(var) - 1] = 0;

		var = trim(var);
		if (val[strlen(val) - 1] == ';')
			val[strlen(val) - 1] = 0;
		val = trim(val);

		vtype = variable_get_type(var, NULL);
		if ((vtype == TYPE_INT) || (vtype == TYPE_LONG)) {
			char tmp[32] = { 0 };
			long value = atol(variable_get_element_as_string(var, NULL));

			if (op == '+')
				value += atol(val);
			else
			if (op == '-')
				value -= atol(val);
			else
			if (op == '*')
				value *= atol(val);
			else
			if (op == '%')
				value %= atol(val);
			else
			if (op == '/')
				value /= atol(val);

			snprintf(tmp, sizeof(tmp), "%ld", value);
			variable_set_deleted(var, 1);
			variable_add(var, tmp, TYPE_QSCRIPT, -1, vtype);
			ret = 0;
		}
		else
			ret = -EINVAL;

		free(var);
		free(val);
		free_tokens(t);
		return ret;
	}
	else
	/* Assignment */
	if (strstr(buf, "=") != NULL) {
		tTokenizer t;

		DPRINTF("%s: Assignment found\n", __FUNCTION__);

		t = tokenize(buf, "=");
		char *val = strdup( trim(t.tokens[1]) );
		if (val[strlen(val) - 1] == ';') {
			val[strlen(val) - 1] = 0;

			if (is_numeric(val) || is_string(val)) {
				if (is_string(val)) {
					*val++;
					val[strlen(val) - 1] = 0;
				}
				if (variable_add(trim(t.tokens[0]), val, TYPE_QSCRIPT, -1, gettype(val)) < 0) {
					desc_printf(gIO, gFd, "Cannot set new value to variable %s\n", trim(t.tokens[0]));
					ret = -EEXIST;
				}
				else
					ret = 0;
			}
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

				ret = 0;
			}
			else
				ret = -EINVAL;
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

		ret = 0;
	}
	else
		DPRINTF("%s: Not implemented yet\n", __FUNCTION__);

	return ret;
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
