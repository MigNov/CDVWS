#include "cdvws.h"

#ifdef DEBUG_CONFIG
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/config      ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int config_initialize(void)
{
	DPRINTF("%s: Base directory set to '%s'\n", __FUNCTION__, basedir);

	DPRINTF("%s: Initializating configuration space\n", __FUNCTION__);

        configVars = NULL;
        numConfigVars = 0;

	return 0;
}

void config_variable_free(int idx)
{
	if ((idx < 0) || (idx > numConfigVars))
		return;

	configVars[idx].filename = utils_free("configVars[].filename", configVars[idx].filename);
	configVars[idx].space = utils_free("configVars[].space", configVars[idx].space);
	configVars[idx].key = utils_free("configVars[].key", configVars[idx].key);
	configVars[idx].value = utils_free("configVars[].value", configVars[idx].value);
	configVars[idx].fullkey = utils_free("configVars[].fullkey", configVars[idx].fullkey);

/*
	configVars[idx].filename = NULL;
	configVars[idx].space = NULL;
	configVars[idx].key = NULL;
	configVars[idx].value = NULL;
	configVars[idx].fullkey = NULL;
*/
}

int config_variable_add(char *filename, char *space, char *key, char *value, char *fullkey)
{
	int idx = -1;
	int increment = 1;

	DPRINTF("%s: Adding configuration variable '%s' to configuration space '%s', value is '%s'\n", __FUNCTION__,
			fullkey, (space != NULL) ? space : "<all>", value);

	if (configVars == NULL) {
		configVars = (tConfigVariable *)utils_alloc( "config.config_variable_add", sizeof(tConfigVariable) );
		numConfigVars = 0;
	}
	else {
		if ((idx = config_variable_get_idx(space, key)) < 0)
			configVars = (tConfigVariable *)realloc( configVars, (numConfigVars + 1) *
				sizeof(tConfigVariable) );
		else
			config_variable_free(idx);
	}

	if (configVars == NULL) {
		DPRINTF("%s: Insufficient memory space\n", __FUNCTION__);
		return -ENOMEM;
	}

	if (idx > -1)
		increment = 0;
	else
		idx = numConfigVars;

	configVars[idx].filename = strdup(filename);
	configVars[idx].space = strdup(space);
	configVars[idx].key = strdup(key);
	configVars[idx].value = strdup(value);
	configVars[idx].fullkey = strdup(fullkey);

	if (increment)
		numConfigVars++;
	return 0;
}

void config_variable_dump(char *space)
{
	int i;

	if (numConfigVars == 0) {
		dump_printf("%s: No configVar is defined for configuration space %s\n", __FUNCTION__,
			(space != NULL) ? space : "<all>");
		return;
	}

	dump_printf("\nConfigVars dump for configuration space %s (%d):\n", (space != NULL) ? space : "<all>",
		numConfigVars);
	for (i = 0; i < numConfigVars; i++) {
		if ((space == NULL) || (strstr(configVars[i].space, space) != NULL)) {
			dump_printf("\tConfigVar #%d:\n", i+1);
			dump_printf("\t\tSource:\t'%s'\n", configVars[i].filename);
			if (space == NULL)
				dump_printf("\t\tSpace:\t'%s'\n", configVars[i].space);
			dump_printf("\t\tKey:\t'%s' (full '%s')\n", configVars[i].key, configVars[i].fullkey);
			dump_printf("\t\tValue:\t'%s'\n", configVars[i].value);
		}
	}
}

int config_variable_get_idx(char *space, char *key)
{
	int i;

	DPRINTF("%s: Getting configuration data for key '%s' from space '%s'\n", __FUNCTION__, key,
		(space != NULL) ? space : "<all>");

	for (i = 0; i < numConfigVars; i++) {
		if (((space == NULL) || (strstr(configVars[i].space, space) != NULL))
			&& (strcmp(configVars[i].key, key) == 0)) {
			DPRINTF("%s: Configuration data found for key '%s' on space '%s' => '%s' (index %d)\n",
				__FUNCTION__, key, (space != NULL) ? space : "<all>", configVars[i].value, i);
			return i;
		}
	}

	DPRINTF("%s: No configuration data found for key '%s' on space '%s'\n", __FUNCTION__,
			key, (space != NULL) ? space : "<all>");
	return -1;
}

char *config_variable_get(char *space, char *key)
{
	int idx = -1;

	if ((idx = config_variable_get_idx(space, key)) < 0)
		return NULL;

	return strdup(configVars[idx].value);
}

char *config_get(char *space, char *key)
{
	return config_variable_get(space, key);
}

int config_handle_line(char *directory, char *space, char *line)
{
	char dir[BUFSIZE] = { 0 };
	char path[BUFSIZE] = { 0 };
	int ret = -ENOENT;
	tTokenizer t;

	t = tokenize(line, " ");
	if (t.numTokens > 0) {
		if (strcmp(t.tokens[0], "include") == 0) {
			if (t.numTokens < 2)
				ret = -EINVAL;
			else {
				snprintf(dir, sizeof(dir), "%s/%s", directory, t.tokens[1]);

				struct dirent *de = NULL;
				DIR *d = opendir(dir);
				if (d == NULL)
					ret = -EBADF;
				else {
					while ((de = readdir(d)) != NULL) {
						if (strstr(de->d_name, ".config") != NULL) {
							snprintf(path, sizeof(path), "%s/%s", dir,
								de->d_name);

							config_load(path, space);
						}
					}

					ret = 0;
				}
				closedir(d);
			}
		}
		else
		if (strcmp(t.tokens[0], "load_module") == 0) {
			if (t.numTokens < 2)
				ret = -EINVAL;
			else {
				snprintf(path, sizeof(path), "%s/mod_%s.so", modules_get_directory(),
					t.tokens[1]);
				DPRINTF("%s: Loading module '%s'\n", __FUNCTION__, t.tokens[1]);

				ret = module_load(path);
			}
		}
	}
	free_tokens(t);

	return ret;
}

void parse_variable(char *cfg_filename, char *line, char *space)
{
	char space2[BUFSIZE] = { 0 };
	char *value = NULL;
	char *key = NULL;
	tTokenizer t;

	/* Space2 = space + '.' */
	if (space != NULL)
		snprintf(space2, sizeof(space2), "%s.", space);

	if ((strncmp(line, space2, strlen(space2)) == 0) ||
		((space == NULL) && (strstr(line, ".") != NULL))) {
		t = tokenize(line, "=");
		if (t.numTokens != 2) {
			DPRINTF("%s: Warning! There are %d tokens instead of 2. Ignoring line: '%s'\n",
				__FUNCTION__, t.numTokens, line);
			return;
		}
		key   = strdup(trim(t.tokens[0]));
		value = strdup(trim(t.tokens[1]));
		free_tokens(t);

		if (space == NULL) {
			space = strdup(key);
			space[ strlen(space) - strlen(strchr(space, '.')) ] = 0;
			snprintf(space2, sizeof(space2), "%s.", space);
		}

		if (config_variable_add( cfg_filename, space, key + strlen(space2), value, key) != 0)
			fprintf(stderr, "Warning: Cannot add configuration variable '%s'\n", key);
	}
}

int config_load(char *filename, char *space)
{
	FILE *fp = NULL;
	char *directory = NULL;
	char *cfg_filename = NULL;
	char line[BUFSIZE] = { 0 };
	int lineNum = 1;
	int err = 0;
	tTokenizer t;

	directory = dirname(strdup(filename));
	cfg_filename = basename(strdup(filename));

	fp = fopen(filename, "r");
	if (fp == NULL) {
		int err = errno;
		DPRINTF("%s: Cannot open file '%s' for reading\n", __FUNCTION__, filename);
		return -err;
	}

        while (!feof(fp)) {
		memset(line, 0, sizeof(line));
                fgets(line, sizeof(line), fp);
		if (line[strlen(line) - 1] == '\n')
			line[strlen(line) - 1] = 0;

		if ((strlen(line) > 0) && (line[0] != '#')) {
			t = tokenize(line, " ");
			if (t.numTokens > 0) {
				if ((err = config_handle_line(directory, space, line)) != 0) {
					if (strchr(line, '.') != NULL)
						parse_variable(cfg_filename, line, space);
					else
						fprintf(stderr, "Directive processing failed on line %d "
								"(%s), error code %d (%s)\n",
							lineNum, line, err, strerror(-err));
				}
			}
			free_tokens(t);
		}

		lineNum++;
        }
	fclose(fp);

	DPRINTF("%s: Configuration data for configuration space '%s' loaded from '%s'\n", __FUNCTION__,
		(space != NULL) ? space : "<all>", filename);
	return 0;
}

char* config_read(const char *filename, char *key)
{
        FILE *fp;
        char line[BUFSIZE];

	DPRINTF("%s: Opening file '%s' to read key '%s'\n", __FUNCTION__, filename, key);

        fp = fopen(filename, "r");
        if (fp == NULL) {
		DPRINTF("%s: Cannot open file '%s' for reading\n", __FUNCTION__, filename);
                return NULL;
	}

        while (!feof(fp)) {
                fgets(line, sizeof(line), fp);

                if (strncmp(line, key, strlen(key)) == 0) {
                        char *tmp = strdup( line + strlen(key) + 3 );
                        if (tmp[strlen(tmp) - 1] == '\n')
                                tmp[strlen(tmp) - 1] = 0;

                        return tmp;
                }
        }
        fclose(fp);

        return NULL;
}

void config_free(void)
{
	int i;

	DPRINTF("%s: Freeing configuration data\n", __FUNCTION__);

	for (i = 0; i < numConfigVars; i++) {
		//configVars[i].filename = utils_free("configVars[].filename",configVars[i].filename);
		configVars[i].space = utils_free("configVars[].space", configVars[i].space);
		configVars[i].key = utils_free("configVars[].key", configVars[i].key);
		configVars[i].value = utils_free("configVars[].value", configVars[i].value);
		configVars[i].fullkey = utils_free("configVars[].fullkey", configVars[i].fullkey);
	}

	numConfigVars = 0;

	DPRINTF("%s: Configuration data freed\n", __FUNCTION__);
}

