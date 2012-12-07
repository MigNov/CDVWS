#include "cdvws.h"

#ifdef DEBUG_SCRIPTING
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/scripting   ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int _script_builtin_function(char *var, char *fn, char *args)
{
	int ret = 0;
	struct timespec ts;
	struct timespec tse;

	if (_perf_measure)
		ts = utils_get_time( TIME_CURRENT );

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
	if (strcmp(fn, "enable_perf") == 0) {
		int enable = get_boolean(args);

		if ((enable == 0) || (enable == 1)) {
			DPRINTF("%sabling performance measuring\n", enable ? "En" : "Dis");

			_perf_measure = enable;
		}
		else
			DPRINTF("Incorrect setting for performace measuring: %d\n", enable);

		if (_perf_measure)
			ts = utils_get_time( TIME_CURRENT );
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
	if (strcmp(fn, "sleep") == 0) {
		int num = atoi(args);

		DPRINTF("%s: Sleeping for %d seconds...\n", __FUNCTION__, num);
		sleep(num);
	}
	else
	if (strcmp(fn, "dumptype") == 0) {
		char *str = variable_get_type_string(args, "any");

		desc_printf(gIO, gFd, "%s\n", str ? str : "<null>");
		str = utils_free("scripting.dumptype.str", str);
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
				var = utils_free("scripting.print.var", var);
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
			if (t.numTokens == 1) {
				char *instr = NULL;

				instr = strdup(t.tokens[0]);
				while (strstr(instr, "\\n") != NULL)
					instr = replace(instr, "\\n", "\n");

				desc_printf(gIO, gFd, "%s", instr);
				instr = utils_free("scripting.printf.instr", instr);
			}
			else
			if (t.numTokens == 2) {
				tTokenizer t2;
				char *instr = NULL;
				char *vars = NULL;

				instr = strdup( t.tokens[0] );
				vars = strdup( t.tokens[1] + 1 );

				t2 = tokenize(vars, ",");
				for (i = 0; i < t2.numTokens; i++) {
					DPRINTF("%s: Replacing variable %s\n", __FUNCTION__, trim(t2.tokens[i]));
					char *tmp = variable_get_element_as_string(trim(t2.tokens[i]), NULL);

					if (tmp != NULL)
						instr = replace(instr, "%s", tmp);
					else {
						instr = replace(instr, "%s", "NULL");
						DPRINTF("%s: Variable \"%s\" not found\n", __FUNCTION__, trim(t2.tokens[i]));
					}
				}

				while (strstr(instr, "\\n") != NULL)
					instr = replace(instr, "\\n", "\n");

				desc_printf(gIO, gFd, "%s", instr);
				free_tokens(t2);
			}
			else {
				free_tokens(t);
				ret = -EINVAL;
				goto cleanup;
			}

			free_tokens(t);
		}
		else {
			desc_printf(gIO, gFd, "Invalid syntax for printf()\n");
			ret = -EINVAL;
			goto cleanup;
		}
	}
	else
	if (strcmp(fn, "idb_dump_query_set") == 0) {
		idb_results_show( gIO, gFd, idb_get_last_select_data() );
	}
	else
	if (strcmp(fn, "idb_query") == 0) {
		char *filename = NULL;
		char *query = NULL;
		tTokenizer t;
		int i;

		t = tokenize(args, "\"");
		if (t.numTokens > 1) {
			int num = 0;
			for (i = 0; i < t.numTokens; i++) {
				if (strcmp(trim(t.tokens[i]), ",") != 0) {
					if (num == 0)
						filename = strdup(t.tokens[i]);
					else
					if (num == 1)
						query = strdup(t.tokens[i]);
					num++;
				}
			}
		}
		free_tokens(t);

		if (((filename == NULL) || (query == NULL)) && (args[0] == '@')) {
			*args++;

			DPRINTF("Reading query file '%s'\n", args);
			if (access(args, R_OK) == 0) {
				FILE *fp = NULL;
				char buf[BUFSIZE];

				fp = fopen(args, "r");
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
							desc_printf(gIO, gFd, "Query '%s' returned with code %d\n",
								buf, ret);
						}
					}

					desc_printf(gIO, gFd, "%d queries processed\n", num);
					fclose(fp);
				}
				else
					desc_printf(gIO, gFd, "Error: Cannot open file %s for reading\n",
						args);
			}
			else
				desc_printf(gIO, gFd, "Error: Cannot access file %s\n", t.tokens[1]);
		}
		else
		if ((filename != NULL) && (query != NULL)) {
			char tmp[4096] = { 0 };
			snprintf(tmp, sizeof(tmp), "INIT %s", filename);

			ret = 0;
			if (idb_query(tmp) != 0) {
				DPRINTF("Error while trying to initialize file '%s'\n", filename);
				ret = -EIO;
			}
			if (idb_query(query) != 0) {
				DPRINTF("Error while running query '%s'\n", query);
				ret = -EIO;
			}
			else
			if ((strncmp(query, "SELECT", 6) == 0) || (strcmp(query, "SHOW TABLES") == 0))
				idb_results_show( gIO, gFd, idb_get_last_select_data() );

			idb_query("COMMIT");
			idb_query("CLOSE");
		}
	}
	else {
		ret = -EINVAL;
		goto cleanup;
	}

cleanup:
	if (_perf_measure) {
		tse = utils_get_time( TIME_CURRENT );
		desc_printf(gIO, gFd, "\nPERF: Function %s() was running for %.3f microseconds (%.3f ms)\n",
			fn, get_time_float_us( tse, ts ), get_time_float_us(tse, ts) / 1000.);
	}

	return ret;
}

int script_process_line(char *buf)
{
	int ret = -ENOTSUP;
	struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };
	struct timespec tse;

	if (_perf_measure)
		ts = utils_get_time( TIME_CURRENT );

	/* Skip if it's a one line comment (more to be implemented) */
	if (is_comment(buf) == 1) {
		DPRINTF("%s: Found comment, skipping\n", __FUNCTION__);
		ret = 0;
		goto cleanup;
	}
	else
	if (strcmp(buf, "else {") == 0) {
		/* Reverse the condition */

		if ((_script_in_condition_and_met == 1)
			|| (_script_in_condition_and_met == -10))
			_script_in_condition_and_met = 0;
		else
		if ((_script_in_condition_and_met == 0)
			|| (_script_in_condition_and_met == -20))
			_script_in_condition_and_met = 1;
		else
		if (_script_in_condition_and_met != -1) {
			DPRINTF("%s: Invalid state for else statement\n", __FUNCTION__);

			ret = -EINVAL;
			goto cleanup;
		}

		ret = 0;
		goto cleanup;
	}
	else
	if (strcmp(buf, "}") == 0) {
		_script_in_condition_and_met = (_script_in_condition_and_met == 1) ? -10 : -20;
		ret = 0;
		goto cleanup;
	}

	if (_script_in_condition_and_met < -1)
		_script_in_condition_and_met = 1;

	if (_script_in_condition_and_met == 0) {
		ret = 0;
		goto cleanup;
	}

	/* Comparison with no ternary operator support... yet */
	if (regex_match("if \\(([^(]*)([^)]*)\\)", buf)) {
		if (regex_match("if \\(([^(]*) == ([^)]*)\\) {", buf)) {
			char **matches = NULL;
			int i, num_matches;
			char *val = NULL;

			matches = (char **)utils_alloc( "scripting.script_process_line.matches", sizeof(char *) );
			_regex_match("if \\(([^(]*) == ([^)]*)\\) {", buf, matches, &num_matches);

			if (num_matches >= 2)
				_script_in_condition_and_met = (valcmp(matches[0], matches[1]) == 0) ? 1 : 0;

			for (i = 0; i < num_matches; i++)
				matches[i] = utils_free("scripting.condition.matches[]", matches[i]);
			matches = utils_free("scripting.condition.matches", matches);
		}
	}
	else
	/* Definition */
	if (strncmp(buf, "define ", 7) == 0) {
		tTokenizer t;

		t = tokenize(buf + 7, " ");
		if (t.numTokens != 2) {
			ret = -EINVAL;
			goto cleanup;
		}
		if (variable_create(trim(t.tokens[0]), trim(t.tokens[1])) != 1) {
			ret = -EIO;
			goto cleanup;
		}
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
		if (t.numTokens != 2) {
			ret = -EINVAL;
			goto cleanup;
		}

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

		var = utils_free("scripting.operators.var", var);
		val = utils_free("scripting.operators.val", val);
		free_tokens(t);
		return ret;
	}
	else
	/* Assignment */
	if (strstr(buf, "=") != NULL) {
		tTokenizer t;

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
				if (t2.tokens[t2.numTokens - 1][strlen(t2.tokens[t2.numTokens - 1]) - 1] != ')') {
					ret = -EINVAL;
					goto cleanup;
				}

				t2.tokens[t2.numTokens - 1][strlen(t2.tokens[t2.numTokens - 1]) - 1] = 0;
				fn = strdup(t2.tokens[0]);

				if (t2.numTokens > 1)
					args = strdup(t2.tokens[1]);
				if (args != NULL) {
					char args2[1024] = { 0 };

					snprintf(args2, sizeof(args2), "%s", args + 1);
					args2[ strlen(args2) - 1] = 0;
					args = utils_free("scripting.function-call.args", args);

					args = strdup( args2 );
				}
				free_tokens(t2);

				if (_script_builtin_function(trim(t.tokens[0]), fn, args) != 0)
					DPRINTF("Function %s doesn't seem to be builtin, we should try user-defined function\n", fn);

				DPRINTF("%s: Should be a function with return value\n", __FUNCTION__);
				args = utils_free("scripting.function-call.args", args);
				fn = utils_free("scripting.function-call.fn", fn);

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
		if (t2.tokens[t2.numTokens - 1][strlen(t2.tokens[t2.numTokens - 1]) - 1] != ';') {
			ret = -EINVAL;
			goto cleanup;
		}

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

		args = utils_free("scripting.function-call.args", args);
		fn = utils_free("scripting.function-call.fn", fn);

		ret = 0;
	}
	else
		DPRINTF("%s: Not implemented yet\n", __FUNCTION__);

cleanup:
	if ((_perf_measure) && ((ts.tv_nsec > 0) && (ts.tv_sec > 0)) && (_script_in_condition_and_met > 0)) {
		tse = utils_get_time( TIME_CURRENT );
		desc_printf(gIO, gFd, "PERF: Line \"%s\" was being processed for %.3f microseconds (%.3f ms)\n\n",
			buf, get_time_float_us( tse, ts ), get_time_float_us(tse, ts) / 1000.);
	}

	return ret;
}

int run_script(char *filename)
{
	FILE *fp;
	int opened = 0;
	char buf[4096] = { 0 };
	struct timespec ts = utils_get_time( TIME_CURRENT );
	struct timespec tse;

	if (access(filename, R_OK) != 0)
		return -ENOENT;

	_script_in_condition_and_met = -1;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return -EPERM;

	if (gHttpHandler)
		http_host_header(gIO, gFd, HTTP_CODE_OK, gHost, "text/html", NULL, myRealm, 0);

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

	fclose(fp);

	idb_free_last_select_data();

	if (_perf_measure) {
		tse = utils_get_time( TIME_CURRENT );
		desc_printf(gIO, gFd, "PERF: File \"%s\" has been processed in %.3f microseconds (%.3f ms)\n\n",
			basename(filename), get_time_float_us( tse, ts ), get_time_float_us(tse, ts) / 1000.);
	}

	variable_dump();
	variable_free_all();
	return 0;
}

#ifdef USE_SSL
void script_set_descriptors(BIO *io, int fd, short httpHandler)
{
	gIO = io;
	gFd = fd;
	gHttpHandler = httpHandler;
}
#else
void script_set_descriptors(void *io, int fd, short httpHandler)
{
	gIO = NULL;
	gFd = fd;
	gHttpHandler = httpHandler;
}
#endif
