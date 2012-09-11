#define DEBUG_XMLRPC

#include "cdvws.h"

#ifdef DEBUG_XMLRPC
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/xmlrpc      ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

char *_xlastElement;
char **_xlastElementNames;
int _xlastElementNames_num;
int _xIdParent;

typedef struct tXmlRPCVars {
	int id;
	int type;
	char *name;
	short shValue;
	int iValue;
	char *sValue;
	double dValue;
	long dtValue;
	int idParent;
} tXmlRPCVars;

tXmlRPCVars *_xmlrpc_vars;
int _xmlrpc_vars_num;

#define	XMLRPC_TYPE_INT		0x00
#define	XMLRPC_TYPE_BOOL	0x01
#define XMLRPC_TYPE_STRING	0x02
#define	XMLRPC_TYPE_DOUBLE	0x03
#define XMLRPC_TYPE_DATETIME	0x04
#define XMLRPC_TYPE_BASE64	0x05
#define	XMLRPC_TYPE_STRUCT	0x06

void xmlrpc_variable_dump(char *methodName)
{
	int i;

	if (_xmlrpc_vars == NULL)
		return;

	if (methodName != NULL)
		DPRINTF("XMLRPC Method name: %s\n", methodName);

	DPRINTF("XMLRPC Variable dump\n");
	for (i = 0; i < _xmlrpc_vars_num; i++) {
		DPRINTF("Variable #%d:\n", i + 1);
		DPRINTF("\tID: %d\n", _xmlrpc_vars[i].id);
		DPRINTF("\tName: %s\n", _xmlrpc_vars[i].name ? _xmlrpc_vars[i].name : "<null>");
		DPRINTF("\tID Parent: %d\n", _xmlrpc_vars[i].idParent);

		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_INT)
			DPRINTF("\tValue: %d (int)\n", _xmlrpc_vars[i].iValue);
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_BOOL)
			DPRINTF("\tValue: %s (boolean)\n", (_xmlrpc_vars[i].shValue == 1) ? "true" : "false");
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_STRING)
			DPRINTF("\tValue: %s (string)\n", _xmlrpc_vars[i].sValue);
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_DOUBLE)
			DPRINTF("\tValue: %.f (double)\n", _xmlrpc_vars[i].dValue);
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_DATETIME)
			DPRINTF("\tValue: <datetime> timestamp = %ld, date/time = %s",
				_xmlrpc_vars[i].dtValue, ctime(&_xmlrpc_vars[i].dtValue));
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_BASE64)
			DPRINTF("\tValue: %s <base64>\n", _xmlrpc_vars[i].sValue);
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_STRUCT)
			DPRINTF("\tValue: <struct>\n");
	}
}

void xmlrpc_variable_free_all(void)
{
	int i;

	if (_xmlrpc_vars == NULL)
		return;

	for (i = 0; i < _xmlrpc_vars_num; i++) {
		_xmlrpc_vars[i].id = 0;
		_xmlrpc_vars[i].idParent = 0;
		_xmlrpc_vars[i].iValue = 0;
		_xmlrpc_vars[i].shValue = 0;
		free(_xmlrpc_vars[i].sValue);
		_xmlrpc_vars[i].sValue = NULL;
		_xmlrpc_vars[i].dValue = 0;
	}
	_xmlrpc_vars_num = 0;
}

void xmlrpc_put_tabs(char *tmp, int tmp_len, int level)
{
	int i;

	for (i = 0; i < level; i++)
		cdvPrintfAppend(tmp, tmp_len, "\t");
}

void xmlrpc_format_params(char *tmp, int tmp_len, tXmlRPCVars *xmlrpc_vars, int xmlrpc_vars_num, int idParent, int level)
{
	int i;

	if (xmlrpc_vars == NULL)
		return;

	for (i = 0; i < xmlrpc_vars_num; i++) {
		if (xmlrpc_vars[i].idParent == idParent) {
			xmlrpc_put_tabs(tmp, tmp_len, level);

			if (level == 2) {
				cdvPrintfAppend(tmp, tmp_len, "<param>\n");

				if (xmlrpc_vars[i].name != NULL) {
					xmlrpc_put_tabs(tmp, tmp_len, level);
					cdvPrintfAppend(tmp, tmp_len, "\t<struct>\n");
					xmlrpc_put_tabs(tmp, tmp_len, level);
					cdvPrintfAppend(tmp, tmp_len, "\t\t<member>\n");
					xmlrpc_put_tabs(tmp, tmp_len, level);
					cdvPrintfAppend(tmp, tmp_len, "\t\t\t<name>%s</name>\n", xmlrpc_vars[i].name);
					level += 2;
				}
			}
			else {
				cdvPrintfAppend(tmp, tmp_len, "<member>\n");
				xmlrpc_put_tabs(tmp, tmp_len, level);
				cdvPrintfAppend(tmp, tmp_len, "\t<name>%s</name>\n", xmlrpc_vars[i].name);
			}

			xmlrpc_put_tabs(tmp, tmp_len, level);
			cdvPrintfAppend(tmp, tmp_len, "\t<value>");

			if (xmlrpc_vars[i].type == XMLRPC_TYPE_INT)
				cdvPrintfAppend(tmp, tmp_len, "<int>%d</int>", xmlrpc_vars[i].iValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_BOOL)
				cdvPrintfAppend(tmp, tmp_len, "<boolean>%d</boolean>", xmlrpc_vars[i].shValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_STRING)
				cdvPrintfAppend(tmp, tmp_len, "<string>%s</string>", xmlrpc_vars[i].sValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_DOUBLE)
				cdvPrintfAppend(tmp, tmp_len, "<double>%.f</double>", xmlrpc_vars[i].dValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_DATETIME) {
				struct tm *tm;
				char buf[256] = { 0 };

				tm = localtime(&xmlrpc_vars[i].dtValue);
				strftime(buf, sizeof(buf), "%Y%m%dT%X", tm);
				cdvPrintfAppend(tmp, tmp_len, "<datetime.iso8601>%s</datetime.iso8601>", buf);
			}
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_BASE64) {
				size_t len = strlen(xmlrpc_vars[i].sValue);
				unsigned char *xtmp = wrap_mincrypt_base64_encode((unsigned char *)
						xmlrpc_vars[i].sValue, &len);

				if (xtmp != NULL) {
					cdvPrintfAppend(tmp, tmp_len, "<base64>%s</base64>", xtmp);
					free(xtmp);
				}
				else {
					/* Try to use `base64` from coreutils package if it failed above */
					if (xmlrpc_vars[i].sValue != NULL) {
						char cmd[1024] = { 0 };
						char s[1024] = { 0 };
						FILE *fp;

						snprintf(cmd, sizeof(cmd), "echo \"%s\" | tr -d \'\\n\' | base64", xmlrpc_vars[i].sValue);
						fp = popen(cmd, "r");
						fgets(s, 1024, fp);
						fclose(fp);

						if (s[strlen(s) - 1] == '\n')
							s[strlen(s) - 1] = 0;

						cdvPrintfAppend(tmp, tmp_len, "<base64>%s</base64>", s);
					}
					else
						cdvPrintfAppend(tmp, tmp_len, "<base64></base64>");
				}
			}
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_STRUCT) {
				cdvPrintfAppend(tmp, tmp_len, "\n");
				xmlrpc_put_tabs(tmp, tmp_len, level);
				cdvPrintfAppend(tmp, tmp_len, "\t\t<struct>\n");
				xmlrpc_format_params(tmp, tmp_len, xmlrpc_vars, xmlrpc_vars_num, xmlrpc_vars[i].id, level + 3);
				xmlrpc_put_tabs(tmp, tmp_len, level + 2);
				cdvPrintfAppend(tmp, tmp_len, "</struct>\n\t");
				xmlrpc_put_tabs(tmp, tmp_len, level);
			}

			cdvPrintfAppend(tmp, tmp_len, "</value>\n");
			xmlrpc_put_tabs(tmp, tmp_len, level);

			if ((level == 0) || (level == 2))
				cdvPrintfAppend(tmp, tmp_len, "</param>\n");
			else
				cdvPrintfAppend(tmp, tmp_len, "</member>\n");
		}
	}

	if (level == 4)
		cdvPrintfAppend(tmp, tmp_len, "\t\t\t</struct>\n\t\t</param>\n");
}

char *xmlrpc_format_reply(tXmlRPCVars *xmlrpc_vars, int xmlrpc_vars_num)
{
	char tmp[TCP_BUF_SIZE] = { 0 };

	cdvPrintfAppend(tmp, sizeof(tmp), "<?xml version=\"1.0\"?>\n");
	cdvPrintfAppend(tmp, sizeof(tmp), "<methodResponse>\n");

	cdvPrintfAppend(tmp, sizeof(tmp), "\t<params>\n");
	xmlrpc_format_params(tmp, sizeof(tmp), xmlrpc_vars, xmlrpc_vars_num, 0, 2);
	cdvPrintfAppend(tmp, sizeof(tmp), "\t</params>\n");
	cdvPrintfAppend(tmp, sizeof(tmp), "</methodResponse>\n");

	return strdup(tmp);
}

int xmlrpc_variable_add(char *type, char *data)
{
	int id;

	if (_xmlrpc_vars == NULL) {
		_xmlrpc_vars = (tXmlRPCVars *)malloc( sizeof(tXmlRPCVars) );
		_xmlrpc_vars_num = 0;
		_xmlrpc_vars[_xmlrpc_vars_num].id = 1;
	}
	else {
		_xmlrpc_vars = (tXmlRPCVars *)realloc( _xmlrpc_vars, (_xmlrpc_vars_num + 1) * sizeof(tXmlRPCVars) );
		_xmlrpc_vars[_xmlrpc_vars_num].id = _xmlrpc_vars[_xmlrpc_vars_num - 1].id + 1;
	}

	_xmlrpc_vars[_xmlrpc_vars_num].idParent = _xIdParent;
	_xmlrpc_vars[_xmlrpc_vars_num].sValue = NULL;
	if (_xlastElement != NULL)
		_xmlrpc_vars[_xmlrpc_vars_num].name = strdup(_xlastElement);
	else
		_xmlrpc_vars[_xmlrpc_vars_num].name = NULL;

	if ((strcmp(type, "i4") == 0) || (strcmp(type, "int") == 0)) {
		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_INT;
		_xmlrpc_vars[_xmlrpc_vars_num].iValue = atoi(data);
	}
	else
	if (strcmp(type, "boolean") == 0) {
		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_BOOL;
		_xmlrpc_vars[_xmlrpc_vars_num].shValue = ((strcmp(data, "true") == 0) || (strcmp(data, "1") == 0)) ? 1 : 0;
	}
	else
	if (strcmp(type, "string") == 0) {
		while (strstr(data, "<") != NULL)
			data = replace(data, "<", "&lt;");
		while (strstr(data, ">") != NULL)
			data = replace(data, "<", "&gt;");
		while (strstr(data, "&") != NULL)
			data = replace(data, "&", "&amp;");

		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_STRING;
		_xmlrpc_vars[_xmlrpc_vars_num].sValue = strdup(data);
	}
	else
	if (strcmp(type, "double") == 0) {
		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_DOUBLE;
		_xmlrpc_vars[_xmlrpc_vars_num].dValue = atof(data);
	}
	else
	if (strcmp(type, "dateTime.iso8601") == 0) {
		char tmp[4] = { 0 };
		struct tm tm = { 0 };
		time_t timeVal;

		memset(tmp, 0, sizeof(tmp));
		tmp[0] = data[0];
		tmp[1] = data[1];
		tmp[2] = data[2];
		tmp[3] = data[3];
		tm.tm_year = atoi(tmp) - 1900;

		memset(tmp, 0, sizeof(tmp));
		tmp[0] = data[4];
		tmp[1] = data[5];
		tm.tm_mon = atoi(tmp) - 1;

		memset(tmp, 0, sizeof(tmp));
		tmp[0] = data[6];
		tmp[1] = data[7];
		tm.tm_mday = atoi(tmp);

		if (data[8] == 'T') {
			memset(tmp, 0, sizeof(tmp));
			tmp[0] = data[9];
			tmp[1] = data[10];
			tm.tm_hour = atoi(tmp);

			memset(tmp, 0, sizeof(tmp));
			tmp[0] = data[12];
			tmp[1] = data[13];
			tm.tm_min = atoi(tmp);

			memset(tmp, 0, sizeof(tmp));
			tmp[0] = data[15];
			tmp[1] = data[16];
			tm.tm_sec = atoi(tmp);
		}

		timeVal = mktime(&tm);

		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_DATETIME;
		_xmlrpc_vars[_xmlrpc_vars_num].dtValue = (long)timeVal;
	}
	else
	if (strcmp(type, "base64") == 0) {
		size_t len = strlen(data);
		unsigned char *tmp = wrap_mincrypt_base64_decode((unsigned char *)data, &len);

		if (tmp != NULL) {
			free(data);
			data = strdup((const char *)tmp);
			free(tmp);
		}

		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_BASE64;
		_xmlrpc_vars[_xmlrpc_vars_num].sValue = strdup(data);
	}
	else
	if (strcmp(type, "struct") == 0) {
		_xIdParent = _xmlrpc_vars[_xmlrpc_vars_num].id;
		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_STRUCT;
	}

	id = _xmlrpc_vars[_xmlrpc_vars_num].id;
	_xmlrpc_vars_num++;

	return id;
}

int is_valid_data_type(char *type)
{
	return ((strcmp(type, "i4") == 0) || (strcmp(type, "int") == 0) || (strcmp(type, "boolean") == 0)
		|| (strcmp(type, "string") == 0) || (strcmp(type, "double") == 0) || (strcmp(type, "dateTime.iso8601") == 0)
		|| (strcmp(type, "base64") == 0));
}

void xmlrpc_process_param(xmlDocPtr doc, xmlNodePtr node, int ignore_check, int level)
{
	char *data = NULL;
	
	while (node != NULL) {
        	data = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

		if (data != NULL) {
			if ((!ignore_check) && (!is_valid_data_type((char *)node->name)))
				DPRINTF("WARNING: Invalid data in %s\n", node->name);
			else {
				if (!((_xlastElement != NULL) && (strcmp(data, _xlastElement) == 0)))
					xmlrpc_variable_add((char *)node->name, data);
			}
		}
		else
		if ((node->name != NULL) && (data == NULL)) {
			if ((strcmp((char *)node->name, "member") == 0)) {
				char *tmp = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode->xmlChildrenNode, 1);
				if (tmp != NULL) {
					if (_xlastElement)
						xmlrpc_variable_add("struct", data);
					free(_xlastElement);
					_xlastElement = strdup(tmp);
					free(tmp);
				}
			}

			xmlrpc_process_param(doc, node->xmlChildrenNode,
				(strcmp((char *)node->name, "member") == 0) ? 1 : 0, level + 1);

			if ((strcmp((char *)node->name, "value") == 0)
				|| (_xlastElement != NULL)) {
					if (_xlastElement == NULL)
						_xIdParent = 0;
					free(_xlastElement);
					_xlastElement = NULL;
				}
		}

		free(data);
		data = NULL;

		node = node->next;
	}
}

char *xmlrpc_parse(char *xml)
{
	xmlParserCtxtPtr xp;
	xmlDocPtr doc;
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	xmlNodeSetPtr nodeset;
	char *methodName = NULL;
	char *ret = NULL;
	int i;

	_xmlrpc_vars = NULL;
	_xmlrpc_vars_num = 0;

	_xlastElementNames = NULL;
	_xlastElementNames_num = 0;

	_xIdParent = 0;

	/* We need to strip the new line characters as it's making issues */
	while (strstr(xml, "\n") != NULL)
		xml = replace(xml, "\n", "");

	DPRINTF("Parsing XML:\n%s\n", xml);

	xp = xmlCreateDocParserCtxt( (xmlChar *)xml );
	if (!xp) {
		DPRINTF("Cannot create DocParserCtxt\n");
		return NULL;
	}

	doc = xmlCtxtReadDoc(xp, (xmlChar *)xml, NULL, NULL, 0);
	if (!doc) {
		DPRINTF("Cannot get xmlDocPtr\n");
		return NULL;
	}

	context = xmlXPathNewContext(doc);
	if (!context) {
		DPRINTF("Cannot get new XPath context\n");
		return NULL;
	}

	result = xmlXPathEvalExpression( (xmlChar *)"//methodCall/methodName", context);
	if (!result) {
		xmlXPathFreeContext(context);
		DPRINTF("Cannot evaluate expression\n");
		goto cleanup;
	}
	if(xmlXPathNodeSetIsEmpty(result->nodesetval)){
		xmlXPathFreeObject(result);
		xmlXPathFreeContext(context);
		DPRINTF("Nothing found\n");
		goto cleanup;
	}
	nodeset = result->nodesetval;
	if (nodeset->nodeNr != 1) {
		xmlXPathFreeObject(result);
		xmlXPathFreeContext(context);
		DPRINTF("Invalid number of methodName elements\n");
		goto cleanup;
	}

	methodName = (char *)xmlNodeListGetString(doc, nodeset->nodeTab[0]->xmlChildrenNode, 1);
	xmlXPathFreeObject(result);

	result = xmlXPathEvalExpression( (xmlChar *)"//methodCall/params/param", context);
	nodeset = result->nodesetval;

	for (i = 0; i < nodeset->nodeNr; i++) {
	        xmlrpc_process_param(doc, nodeset->nodeTab[i], 0, 1);
	}

	ret = strdup(methodName);
cleanup:
	xmlXPathFreeObject(result);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return ret;
}

tXmlRPCVars *xmlrpc_process_method(char *methodName, int *num_vars)
{
	DPRINTF("Processing method %s\n", methodName);

	/* Bogus, just for testing (for now) */
	if (num_vars != NULL)
		*num_vars = _xmlrpc_vars_num;

	return _xmlrpc_vars;
}

char *xmlrpc_process(char *xml)
{
	tXmlRPCVars *xmlrpc_vars = NULL;
	char *methodName = NULL;
	int xmlrpc_varc = 0;
	char *buf = NULL;

	if ((methodName = xmlrpc_parse(xml)) == NULL)
		return NULL;

	xmlrpc_vars = xmlrpc_process_method(methodName, &xmlrpc_varc);
	buf = xmlrpc_format_reply(xmlrpc_vars, xmlrpc_varc);
	xmlrpc_variable_free_all();

	return strdup(buf);
}

