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

void xmlrpc_variable_dump(void)
{
	int i;

	if (_xmlrpc_vars == NULL)
		return;

	DPRINTF("XMLRPC Variable dump\n");
	for (i = 0; i < _xmlrpc_vars_num; i++) {
		DPRINTF("Variable #%d:\n", i + 1);
		DPRINTF("\tID: %d\n", _xmlrpc_vars[i].id);
		DPRINTF("\tName: %s\n", _xmlrpc_vars[i].name);
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
			DPRINTF("\tValue: <datetime>\n");
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_BASE64)
			DPRINTF("\tValue: <base64>\n");
		else
		if (_xmlrpc_vars[i].type == XMLRPC_TYPE_STRUCT)
			DPRINTF("\tValue: <struct>\n");
	}
}

void xmlrpc_put_tabs(int level)
{
	int i;

	for (i = 0; i < level; i++)
		printf("\t");
}

void xmlrpc_format_params(tXmlRPCVars *xmlrpc_vars, int xmlrpc_vars_num, int idParent, int level)
{
	int i;

	if (xmlrpc_vars == NULL)
		return;

	for (i = 0; i < xmlrpc_vars_num; i++) {
		if (xmlrpc_vars[i].idParent == idParent) {
			xmlrpc_put_tabs(level);

			if (level == 2) {
				printf("<param>\n");

				if (xmlrpc_vars[i].name != NULL) {
					xmlrpc_put_tabs(level);
					printf("\t<struct>\n");
					xmlrpc_put_tabs(level);
					printf("\t\t<member>\n");
					xmlrpc_put_tabs(level);
					printf("\t\t\t<name>%s</name>\n", xmlrpc_vars[i].name);
					level += 2;
				}
			}
			else {
				printf("<member>\n");
				xmlrpc_put_tabs(level);
				printf("\t<name>%s</name>\n", xmlrpc_vars[i].name);
			}

			xmlrpc_put_tabs(level);
			printf("\t<value>");

			if (xmlrpc_vars[i].type == XMLRPC_TYPE_INT)
				printf("<int>%d</int>", xmlrpc_vars[i].iValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_BOOL)
				printf("<boolean>%d</boolean>", xmlrpc_vars[i].shValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_STRING)
				printf("<string>%s</string>", xmlrpc_vars[i].sValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_DOUBLE)
				printf("<double>%.f</double>", xmlrpc_vars[i].dValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_DATETIME)
				printf("<datetime>not implemented yet</datetime>");
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_BASE64)
				printf("<base64>%s</base64>", xmlrpc_vars[i].sValue);
			else
			if (xmlrpc_vars[i].type == XMLRPC_TYPE_STRUCT) {
				printf("\n");
				xmlrpc_put_tabs(level);
				printf("\t\t<struct>\n");
				xmlrpc_format_params(xmlrpc_vars, xmlrpc_vars_num, xmlrpc_vars[i].id, level + 3);
				xmlrpc_put_tabs(level + 2);
				printf("</struct>\n\t");
				xmlrpc_put_tabs(level);
			}

			printf("</value>\n");
			xmlrpc_put_tabs(level);

			if ((level == 0) || (level == 2))
				printf("</param>\n");
			else
				printf("</member>\n");
		}
	}

	if (level == 4)
		printf("\t\t\t</struct>\n\t\t</param>\n");
}

void xmlrpc_format_reply(tXmlRPCVars *xmlrpc_vars, int xmlrpc_vars_num)
{
	printf("<?xml version=\"1.0\"?>\n");
	printf("<methodResponse>\n");

	printf("\t<params>\n");
	xmlrpc_format_params(xmlrpc_vars, xmlrpc_vars_num, 0, 2);
	printf("\t<params>\n");
	printf("</methodResponse>\n");
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
	if (_xlastElement != NULL)
		_xmlrpc_vars[_xmlrpc_vars_num].name = strdup(_xlastElement);
	else {
		char tmp[16] = { 0 };
		snprintf(tmp, sizeof(tmp), "%d", _xmlrpc_vars[_xmlrpc_vars_num].id);

		_xmlrpc_vars[_xmlrpc_vars_num].name = strdup(tmp);
	}

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
		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_DATETIME;
		printf("DATETIME: %s\n", data);
	}
	else
	if (strcmp(type, "base64") == 0) {
		_xmlrpc_vars[_xmlrpc_vars_num].type = XMLRPC_TYPE_BASE64;
		printf("BASE64 VALUE: %s\n", data);
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
				DPRINTF("Invalid data in %s\n", node->name);
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

int xmlrpc_parse(char *xml)
{
	xmlParserCtxtPtr xp;
	xmlDocPtr doc;
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	xmlNodeSetPtr nodeset;
	char *methodName = NULL;
	int i, ret = -EINVAL;

	_xmlrpc_vars = NULL;
	_xmlrpc_vars_num = 0;

	_xlastElementNames = NULL;
	_xlastElementNames_num = 0;

	_xIdParent = 0;

	xp = xmlCreateDocParserCtxt( (xmlChar *)xml );
	if (!xp) {
		DPRINTF("Cannot create DocParserCtxt\n");
		return -ENOMEM;
	}

	doc = xmlCtxtReadDoc(xp, (xmlChar *)xml, NULL, NULL, 0);
	if (!doc) {
		DPRINTF("Cannot get xmlDocPtr\n");
		return -ENOMEM;
	}

	context = xmlXPathNewContext(doc);
	if (!context) {
		DPRINTF("Cannot get new XPath context\n");
		return -ENOMEM;
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

	printf("METHOD NAME IS: %s\n", methodName);
	for (i = 0; i < nodeset->nodeNr; i++) {
	        xmlrpc_process_param(doc, nodeset->nodeTab[i], 0, 1);
	}

	xmlrpc_variable_dump();

	//xmlrpc_format_params(0, 0);
	xmlrpc_format_reply(_xmlrpc_vars, _xmlrpc_vars_num);

	ret = 0;
cleanup:
	xmlXPathFreeObject(result);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return ret;
}

