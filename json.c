#include "cdvws.h"

#define DEBUG_JSON

#ifdef DEBUG_JSON
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/json        ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int _j_parentId;
int *_j_ids;
int _j_ids_num;

typedef struct tJSONVars {
	int id;
	int type;
	char *name;
	int iValue;
	char *sValue;
	int idParent;
} tJSONVars;

typedef struct tJSONListVars {
        int id;
	int numVars;
	char **vars;
} tJSONListVars;

tJSONListVars *_json_list_vars;
int _json_list_vars_num;
tJSONVars *_json_vars;
int _json_vars_num;

#define	JSON_TYPE_INT		0x00
#define JSON_TYPE_STRING	0x01
#define	JSON_TYPE_LIST		0x02
#define JSON_TYPE_ARRAY		0x04

void json_variable_dump(void)
{
	int i, j, id = -1;

	if (_json_vars == NULL)
		return;

	DPRINTF("JSON Variable dump\n");
	for (i = 0; i < _json_vars_num; i++) {
		DPRINTF("Variable #%d:\n", i + 1);
		DPRINTF("\tID: %d\n", _json_vars[i].id);
		DPRINTF("\tName: %s\n", _json_vars[i].name ? _json_vars[i].name : "<null>");
		DPRINTF("\tID Parent: %d\n", _json_vars[i].idParent);

		if (_json_vars[i].type == JSON_TYPE_INT)
			DPRINTF("\tValue: %d (int)\n", _json_vars[i].iValue);
		else
		if (_json_vars[i].type == JSON_TYPE_STRING)
			DPRINTF("\tValue: %s (string)\n", _json_vars[i].sValue);
		else
		if (_json_vars[i].type == JSON_TYPE_LIST) {
			DPRINTF("\tValue: <list>\n");

			for (j = 0; j < _json_list_vars_num; j++) {
				if (_json_list_vars[j].id == _json_vars[i].id) {
					id = j;
					break;
				}
			}

			if (id > -1) {
				for (j = 0; j < _json_list_vars[id].numVars; j++) {
					DPRINTF("\t\t#%d: %s\n", j + 1, _json_list_vars[id].vars[j]);
				}

				if (_json_list_vars[id].numVars == 0)
					DPRINTF("\t\t<empty list>\n");
			}
		}
		else
		if (_json_vars[i].type == JSON_TYPE_ARRAY)
			DPRINTF("\tValue: <array>\n");
	}
}

void json_variable_free_all(void)
{
	int i;

	if (_json_vars == NULL)
		return;

	for (i = 0; i < _json_vars_num; i++) {
		_json_vars[i].id = 0;
		_json_vars[i].idParent = 0;
		_json_vars[i].iValue = 0;
		_json_vars[i].sValue = utils_free("json.json_variable_free_all", _json_vars[i].sValue);
		_json_vars[i].sValue = NULL;
	}
	_json_vars_num = 0;

	if (_json_list_vars == NULL)
		return;

	for (i = 0; i < _json_list_vars_num; i++) {
		int j, nv = _json_list_vars[i].numVars;

		for (j = 0; j < nv; j++)
			_json_list_vars[i].vars = utils_free("json.json_variable_free_all", _json_list_vars[i].vars);
	}

	_json_list_vars_num = 0;
}

void json_put_tabs(char *tmp, int tmp_len, int level)
{
	int i;

	for (i = 0; i < level; i++)
		cdvPrintfAppend(tmp, tmp_len, "\t");
}

void json_format_params(char *tmp, int tmp_len, tJSONVars *json_vars, int json_vars_num, int idParent, int level)
{
	int i, cnt = 0, total = 0;

	if (json_vars == NULL)
		return;

	for (i = 0; i < json_vars_num; i++) {
		if (json_vars[i].idParent == idParent)
			total++;
	}

	for (i = 0; i < json_vars_num; i++) {
		if (json_vars[i].idParent == idParent) {
			json_put_tabs(tmp, tmp_len, level);

			cdvPrintfAppend(tmp, tmp_len, "\"%s\": ", json_vars[i].name);

			if (json_vars[i].type == JSON_TYPE_ARRAY) {
				cdvPrintfAppend(tmp, tmp_len, "{\n");
				json_format_params(tmp, tmp_len, json_vars, json_vars_num, json_vars[i].id, level + 1);
				json_put_tabs(tmp, tmp_len, level);
				cdvPrintfAppend(tmp, tmp_len, "}");
			}
			else
			if (json_vars[i].type == JSON_TYPE_INT)
				cdvPrintfAppend(tmp, tmp_len, "%d", json_vars[i].iValue);
			else
			if (json_vars[i].type == JSON_TYPE_STRING)
				cdvPrintfAppend(tmp, tmp_len, "\"%s\"", json_vars[i].sValue);
			else
			if (json_vars[i].type == JSON_TYPE_LIST) {
				int j, id;

				for (j = 0; j < _json_list_vars_num; j++) {
					if (_json_list_vars[j].id == _json_vars[i].id) {
						id = j;
						break;
					}
				}

				if (id > -1) {
					cdvPrintfAppend(tmp, tmp_len, "[ ");
					for (j = 0; j < _json_list_vars[id].numVars; j++) {
						cdvPrintfAppend(tmp, tmp_len, "%s", _json_list_vars[id].vars[j]);

						if (j < _json_list_vars[id].numVars - 1)
							cdvPrintfAppend(tmp, tmp_len, ", ");
					}
					cdvPrintfAppend(tmp, tmp_len, " ]");
				}
			}
			else
				cdvPrintfAppend(tmp, tmp_len, "???");

			if (cnt < total - 1)
				cdvPrintfAppend(tmp, tmp_len, ",");

			cdvPrintfAppend(tmp, tmp_len, "\n");
			cnt++;
		}
	}
}

char *json_format_reply(tJSONVars *json_vars, int json_vars_num)
{
	char tmp[TCP_BUF_SIZE] = { 0 };

	cdvPrintfAppend(tmp, sizeof(tmp), "{");
	json_format_params(tmp, sizeof(tmp), json_vars, json_vars_num, 0, 2);
	cdvPrintfAppend(tmp, sizeof(tmp), "}");

	return strdup(tmp);
}

int _json_variable_list_set(int id, tTokenizer t)
{
	int i;

	if (_json_list_vars == NULL) {
		_json_list_vars = (tJSONListVars *)utils_alloc( "json.json_variable_add", sizeof(tJSONListVars) );
		_json_list_vars_num = 0;
		_json_list_vars[_json_list_vars_num].id = 1;
	}
	else {
		_json_list_vars = (tJSONVars *)realloc( _json_list_vars, (_json_list_vars_num + 1) * sizeof(tJSONListVars) );
		_json_list_vars[_json_list_vars_num].id = _json_list_vars[_json_list_vars_num - 1].id + 1;
	}

	_json_list_vars[_json_list_vars_num].id = id;
	_json_list_vars[_json_list_vars_num].numVars = t.numTokens;

	_json_list_vars[_json_list_vars_num].vars = (char **)malloc(t.numTokens * sizeof(char **));
	for (i = 0; i < t.numTokens; i++)
		_json_list_vars[_json_list_vars_num].vars[i] = strdup(trim(t.tokens[i]));

	_json_list_vars_num++;

	DPRINTF("%s(%d) set to %d elements\n", __FUNCTION__, id, t.numTokens);
	return t.numTokens;
}

int _json_variable_add(char *name, char *data, char *type)
{
	int id;

	if (_json_vars == NULL) {
		_json_vars = (tJSONVars *)utils_alloc( "json.json_variable_add", sizeof(tJSONVars) );
		_json_vars_num = 0;
		_json_vars[_json_vars_num].id = 1;
	}
	else {
		_json_vars = (tJSONVars *)realloc( _json_vars, (_json_vars_num + 1) * sizeof(tJSONVars) );
		_json_vars[_json_vars_num].id = _json_vars[_json_vars_num - 1].id + 1;
	}

	if ((name[0] == '"') && (name[strlen(name) - 1])) {
		*name++;
		name[strlen(name) - 1] = 0;
	}

	_json_vars[_json_vars_num].idParent = _j_parentId;
	_json_vars[_json_vars_num].sValue = NULL;
	_json_vars[_json_vars_num].name = strdup(name);
	id = _json_vars[_json_vars_num].id;

	if ((type == NULL) && (data != NULL) && (strlen(data) > 0)) {
		if ((data[0] == '"') && (data[strlen(data) - 1])) {
			*data++;
			data[strlen(data) - 1] = 0;

			_json_vars[_json_vars_num].type = JSON_TYPE_STRING;
			_json_vars[_json_vars_num].sValue = strdup(data);
		}
		else
		if ((strstr(data, "[") != NULL) && (strstr(data, "]") != NULL)) {
			int i;
			tTokenizer t;

			while (*data++ != '[') ;
			for (i = strlen(data) - 1; i > 0; i--)
				if (data[i] == ']')
					break;
			data[i] = 0;
			data = trim(data);

			t = tokenize(data, ",");
			_json_variable_list_set(id, t);
			free_tokens(t);

			_json_vars[_json_vars_num].type = JSON_TYPE_LIST;
			data = strdup("<list>");
		}
		else {
			_json_vars[_json_vars_num].type = JSON_TYPE_INT;
			_json_vars[_json_vars_num].iValue = atoi(data);
		}
	}
	else {
		if (strcmp(type, "int") == 0) {
			_json_vars[_json_vars_num].type = JSON_TYPE_INT;
			_json_vars[_json_vars_num].iValue = atoi(data);
		}
		else
		if (strcmp(type, "string") == 0) {
			_json_vars[_json_vars_num].type = JSON_TYPE_STRING;
			_json_vars[_json_vars_num].sValue = strdup(data);
		}
		else
		if (strcmp(type, "array") == 0) {
			_j_parentId = _json_vars[_json_vars_num].id;
			_json_vars[_json_vars_num].type = JSON_TYPE_ARRAY;
		}
	}

	_json_vars_num++;

	DPRINTF("%s(%s, %s, %s): Entry with ID %d added\n", __FUNCTION__, name ? name : "<null>",
		data ? data : "<null>", type ? type : "auto", id);

	return id;
}

void json_parse(char *json_string)
{
	char a[2] = { 0 };
	char tmp[1024] = { 0 };
	char *str = json_string;
	char *key = NULL;
	char *prev = NULL;
	int c, level = 0;
	int is_value;
	int last_is_value = 0;
	int token_num = 0;
	int in_element = 0;

	_json_vars = NULL;
	_json_vars_num = 0;
	_j_ids = NULL;
	_j_ids_num = 0;

	_j_parentId = 0;
	while (*str++) {
		c = *str;
		if (((c != ',') && (c != ' ') && (c != '\n') && (c != '\t') && (c != ':')) || in_element) {
			a[0] = c;
			strcat(tmp, a);

			if ((c == '[') || (c == ']'))
				in_element = (c == '[');
		}
		else {
			if (strlen(tmp) > 0) {
				if (tmp[0] == '{') {
					level++;
					_j_ids = id_list_push("json_parse_ids", _j_ids, &_j_ids_num, _j_parentId);
					token_num = 0;
				}
				else
				if (tmp[0] == '}') {
					level--;
					_j_ids = id_list_pop("json_parse_ids", _j_ids, &_j_ids_num, &_j_parentId);
					token_num = 0;
				}
				else {
					is_value = (token_num % 2 == 1);

					if (is_value == 0) {
						free(key);
						key = strdup(tmp);
					}

					token_num++;
					if ((last_is_value == 0) && (is_value == 0) && (prev != NULL))
						_j_parentId = _json_variable_add(prev, NULL, "array");
					else
					if (is_value == 1)
						_json_variable_add(key, tmp, NULL);

					last_is_value = is_value;

					free(prev);
					prev = strdup(tmp);
				}
			}

			memset(tmp, 0, sizeof(tmp));
		}
	}
}

int jsonrpc_process(char *json_string)
{
	json_parse(json_string);

	printf("JSON PARSER TEST RETURNS:\n%s\n", json_format_reply(_json_vars, _json_vars_num));

	json_variable_dump();
	json_variable_free_all();
	return 0;
}

