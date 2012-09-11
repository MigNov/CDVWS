#define DEBUG_SCRIPTING

#include "cdvws.h"

#ifdef DEBUG_SCRIPTING
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/scripting   ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int script_process_line(char *buf)
{
	DPRINTF("BUF: %s\n", buf);

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

			DPRINTF("'%s'\n", val);
			if (is_numeric(val))
				variable_add(trim(t.tokens[0]), val, TYPE_QSCRIPT, -1, gettype(val));
			else
			if (regex_match("([^(]*)([^)]*)", val)) {
				DPRINTF("%s: Should be a function with return value\n", __FUNCTION__);
			}

			for (i = 0; i < t.numTokens; i++) {
				DPRINTF("%d. %s\n", i + 1, trim(t.tokens[i]));
			}
		}

		free_tokens(t);
	}
	else
	if (regex_match("([^(]*)([^)]*)", buf)) {
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

