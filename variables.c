#define DEBUG_VARIABLES

#include "cdvws.h"

#ifdef DEBUG_VARIABLES
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/variables   ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int variable_add(char *name, char *value, int q_type, int idParent, int type)
{
	if (_vars == NULL) {
		_vars = (tVariables *)malloc( sizeof(tVariables) );
		_vars_num = 0;
	}
	else
		_vars = (tVariables *)realloc( _vars, (_vars_num + 1) * sizeof(tVariables) );

	if (_vars == NULL)
		return -ENOMEM;

	DPRINTF("%s: Adding variable %s with value %s (%d); type is %d\n", __FUNCTION__, name, value, idParent, type);

	_vars[_vars_num].id = _vars_num;
	_vars[_vars_num].iValue = 0;
	_vars[_vars_num].lValue = 0;
	_vars[_vars_num].sValue = NULL;
	_vars[_vars_num].q_type = q_type;
	_vars[_vars_num].idParent = idParent;
	if (name == NULL)
		_vars[_vars_num].name = NULL;
	else
		_vars[_vars_num].name = strdup(name);

	_vars[_vars_num].type = type;

	switch (type) {
		case TYPE_INT: _vars[_vars_num].iValue = atoi(value);
				break;
		case TYPE_LONG: _vars[_vars_num].lValue = atol(value);
				break;
		case TYPE_DOUBLE: _vars[_vars_num].dValue = atof( replace(value, ",", ".") );
				break;
		case TYPE_STRING: _vars[_vars_num].sValue = strdup(value);
				break;
		default:
				break;
	}
	
	_vars_num++;
	return _vars_num - 1;
}

char *variable_get_element_as_string(char *el, char *type)
{
	tTokenizer t;
	int i, id = -1;
	char val[1024] = { 0 };

	t = tokenize(el, ".");
	for (i = 0; i < t.numTokens; i++)
		id = variable_lookup_name_idx(t.tokens[i], type, id);

	switch (_vars[id].type) {
		case TYPE_INT: snprintf(val, sizeof(val), "%d", _vars[id].iValue);
				break;
		case TYPE_LONG: snprintf(val, sizeof(val), "%ld", _vars[id].lValue);
				break;
		case TYPE_STRING:
				snprintf(val, sizeof(val), "%s", _vars[id].sValue);
				break;
		case TYPE_DOUBLE:
				snprintf(val, sizeof(val), "%f", _vars[id].dValue);
				break;
		default:
				free_tokens(t);
				return NULL;
	}

	free_tokens(t);

	return strdup(val);
}

void variable_dump(void)
{
	int i;

	for (i = 0; i < _vars_num; i++) {
		DPRINTF("Variable #%d:\n", i + 1);
		DPRINTF("\tId: %d\n", _vars[i].id);
		DPRINTF("\tIdParent: %d\n", _vars[i].idParent);
		DPRINTF("\tVariable source: %s\n", (_vars[i].q_type == TYPE_QPOST) ? "POST Request" :
			((_vars[i].q_type == TYPE_QGET) ? "GET Request" : "SCRIPT Processing"));
		DPRINTF("\tName: %s\n", _vars[i].name ? _vars[i].name : "<null>");

		if (_vars[i].type == TYPE_INT)
			DPRINTF("\tValue: %d (int)\n", _vars[i].iValue);
		else
		if (_vars[i].type == TYPE_LONG)
			DPRINTF("\tValue: %ld (long)\n", _vars[i].lValue);
		else
		if (_vars[i].type == TYPE_STRING)
			DPRINTF("\tValue: %s (string)\n", _vars[i].sValue);
		else
		if (_vars[i].type == TYPE_DOUBLE)
			DPRINTF("\tValue: %f (double)\n", _vars[i].dValue);
		else
		if (_vars[i].type == TYPE_ARRAY)
			DPRINTF("\tValue: <array%s\n", ">");
		else
		if (_vars[i].type == TYPE_STRUCT)
			DPRINTF("\tValue: <struct%s\n", ">");
	}
}

int variable_lookup_name_idx(char *name, char *type, int idParent)
{
	int i;

	for (i = 0; i < _vars_num; i++)
		if ((_vars[i].name != NULL)
			&& (strcmp(_vars[i].name, name) == 0)
			&& (_vars[i].idParent == idParent)
			&& ((
				((type == NULL) || (strcasecmp(type, "any") == 0))
				|| ((_vars[i].q_type == TYPE_QPOST) && (strcasecmp(type, "post") == 0)))
				|| ((_vars[i].q_type == TYPE_QGET) && (strcasecmp(type, "get") == 0))
				))
			return i;

	return -1;
}

void variable_free_all(void)
{
	int i;

	for (i = 0; i < _vars_num; i++) {
		free(_vars[i].name);
		free(_vars[i].sValue);
	}

	_vars_num = 0;
	free(_vars);
	_vars = NULL;
}

