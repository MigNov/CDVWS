#define DEBUG_VARIABLES

#include "cdvws.h"

#ifdef DEBUG_VARIABLES
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/variables   ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int variable_allow_overwrite(char *name, int allow)
{
	if ((allow != 0) && (allow != 1))
		return -EINVAL;

	DPRINTF("Calling %s(%s, %d)\n", __FUNCTION__, name ? name : "<all>", allow);

	if (name == NULL)
		_var_overwrite = allow;
	else {
		int idx = variable_get_idx(name, NULL);
		if (idx < 0)
			return -ENOENT;

		_vars[idx].allow_overwrite = allow;
	}

	return 0;
}

int variable_get_overwrite(char *name)
{
	if (name == NULL)
		return _var_overwrite;
	else {
		int idx = variable_get_idx(name, NULL);

		if (idx < 0)
			return -ENOENT;

		if (_vars[idx].allow_overwrite == -1)
			return _var_overwrite;
		else
			return _vars[idx].allow_overwrite;
	}
}

int variable_set_deleted(char *name, int is_deleted)
{
	if (name == NULL)
		return -EINVAL;

	if ((is_deleted != 0) && (is_deleted != 1))
		return -EINVAL;

	int idx = variable_get_idx(name, NULL);
	if (idx < 0)
		return -ENOENT;

	_vars[idx].deleted = is_deleted;
	return 0;
}

int variable_get_deleted(char *name)
{
	if (name == NULL)
		return 1;

	int idx = variable_get_idx(name, NULL);
	if (idx < 0)
		return 1;

	return _vars[idx].deleted;
}

int variable_set_fixed_type(char *name, char *type)
{
	int tmp, idx;

	if (name == NULL)
		return 0;

	idx = variable_get_idx(name, NULL);
	if (idx < 0)
		return 0;

	if ((type == NULL) || (strcmp(type, "dynamic") == 0)) {
		_vars[idx].fixed_type = 0;
	}
	else {
		tmp = get_type_from_string(type, 0);
		if (tmp < 0)
			return 0;

		_vars[idx].fixed_type = tmp;
	}

	DPRINTF("%s: Variable %s set as fixed to type %s\n", __FUNCTION__, name, type);
	return 1;
}

int variable_get_fixed_type(char *name)
{
	if (name == NULL)
		return 0;

	int idx = variable_get_idx(name, NULL);
	if (idx < 0)
		return -1;

	return _vars[idx].fixed_type;
}

int variable_create(char *name, char *type)
{
	if ((name == NULL) || (type == NULL))
		return -1;

	if (_vars == NULL) {
		_vars = (tVariables *)malloc( sizeof(tVariables) );
		_vars_num = 0;
	}
	else {
		int idx;

		if ((idx = variable_lookup_name_idx(name, NULL, -1)) != -1)
			return -EEXIST;

		_vars = (tVariables *)realloc( _vars, (_vars_num + 1) * sizeof(tVariables) );
	}

	if (_vars == NULL)
		return -ENOMEM;

	/* Type cannot end with semicolon so remove it */
	if (type[strlen(type) - 1] == ';')
		type[strlen(type) - 1] = 0;

	DPRINTF("%s: Creating variable %s; type is %s\n", __FUNCTION__, name, type);

	_vars[_vars_num].id = _vars_num;
	_vars[_vars_num].iValue = 0;
	_vars[_vars_num].lValue = 0;
	_vars[_vars_num].sValue = NULL;
	_vars[_vars_num].q_type = TYPE_QSCRIPT;
	_vars[_vars_num].idParent = -1;
	_vars[_vars_num].deleted = 1;
	_vars[_vars_num].fixed_type = get_type_from_string(type, 1);
	_vars[_vars_num].allow_overwrite = -1;
	if (name == NULL)
		_vars[_vars_num].name = NULL;
	else
		_vars[_vars_num].name = strdup(name);

	_vars[_vars_num].type = type;
	_vars_num++;
	return _vars_num - 1;
}

int variable_set(char *name, char *value, int q_type, int idParent, int type)
{
	int idx, ftype;

	if ((idx = variable_lookup_name_idx(name, NULL, idParent))== -1)
		return -ENOENT;

	/* If the variable exists and has fixed type do *not* allow type override */
	ftype = variable_get_fixed_type(name);
	if ((ftype > 0) && (ftype != type))
		return -ENOTSUP;

        _vars[idx].iValue = 0;
        _vars[idx].lValue = 0;
	_vars[idx].dValue = 0.00;
        _vars[idx].q_type = q_type;
        _vars[idx].idParent = idParent;
	_vars[idx].deleted = 0;

	free(_vars[idx].sValue);
	_vars[idx].sValue = NULL;

	_vars[idx].type = type;

	switch (type) {
		case TYPE_INT: _vars[idx].iValue = atoi(value);
				break;
		case TYPE_LONG: _vars[idx].lValue = atol(value);
				break;
		case TYPE_DOUBLE: _vars[idx].dValue = atof( replace(value, ",", ".") );
				break;
		case TYPE_STRING: _vars[idx].sValue = strdup(value);
				break;
		default:
				break;
	}

	return 0;
}

int variable_add(char *name, char *value, int q_type, int idParent, int type)
{
	if (value == NULL)
		return -1;

	if (_vars == NULL) {
		_vars = (tVariables *)malloc( sizeof(tVariables) );
		_vars_num = 0;
	}
	else {
		int idx;

		if ((idx = variable_lookup_name_idx(name, NULL, idParent)) != -1) {
			if ((variable_get_overwrite(name) == 0) && (variable_get_deleted(name) == 0))
				return -EEXIST;

			return (variable_set(name, value, q_type, idParent, type) == 0) ? idx : -1;
		}

		_vars = (tVariables *)realloc( _vars, (_vars_num + 1) * sizeof(tVariables) );
	}

	if (_vars == NULL)
		return -ENOMEM;

	DPRINTF("%s: Adding variable %s with value %s (%d); type is %d\n", __FUNCTION__, name, value, idParent, type);

	_vars[_vars_num].id = _vars_num;
	_vars[_vars_num].iValue = 0;
	_vars[_vars_num].lValue = 0;
	_vars[_vars_num].sValue = NULL;
	_vars[_vars_num].q_type = q_type;
	_vars[_vars_num].idParent = idParent;
	_vars[_vars_num].deleted = 0;
	_vars[_vars_num].fixed_type = 0;
	_vars[_vars_num].allow_overwrite = -1;
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

int variable_get_idx(char *el, char *type)
{
	tTokenizer t;
	int i, id = -1;

	t = tokenize(el, ".");
	for (i = 0; i < t.numTokens; i++)
		id = variable_lookup_name_idx(t.tokens[i], type, id);
	free_tokens(t);

        return id;
}

int variable_get_type(char *el, char *type)
{
	int id = variable_get_idx(el, type);

	if (id < 0)
		return -EINVAL;

	return _vars[id].type;
}

char *variable_get_type_string(char *el, char *type)
{
	int id = variable_get_type(el, type);

	if (id < 0)
		return NULL;

	return get_type_string(id);
}

char *variable_get_element_as_string(char *el, char *type)
{
	tTokenizer t;
	int i, id = -1;
	char val[1024] = { 0 };

	t = tokenize(el, ".");
	for (i = 0; i < t.numTokens; i++)
		id = variable_lookup_name_idx(t.tokens[i], type, id);

	/* If there's no such variable then return NULL */
	if (id == -1)
		return NULL;

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
		DPRINTF("\tVariable source: %s\n",
			(_vars[i].q_type == TYPE_QPOST) ? "POST Request" :
			((_vars[i].q_type == TYPE_QGET) ? "GET Request" :
			((_vars[i].q_type == TYPE_MODULE) ? "MODULE Processing" : "SCRIPT Processing")));
		DPRINTF("\tName: %s\n", _vars[i].name ? _vars[i].name : "<null>");
		DPRINTF("\tAllow overwrite: %d (local %d)\n", variable_get_overwrite(_vars[i].name),
				 _vars[i].allow_overwrite);
		DPRINTF("\tDeleted: %d\n", _vars[i].deleted);
		DPRINTF("\tFixed to type: %s\n", (_vars[i].fixed_type > 0) ?
			get_type_string(_vars[i].fixed_type) : "<none>");

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
		else
			DPRINTF("\tValue: %s\n", "<unset>");
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
				|| ((_vars[i].q_type == TYPE_MODULE) && (strcasecmp(type, "module") == 0))
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

