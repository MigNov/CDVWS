#define DEBUG_XML
#define HAVE_END_AUTODUMPS

#define ROOT_ELEMENT_DEFS "//page-definition"

#include "cdvws.h"
#include <libxml/parser.h>
#include <libxml/xpath.h>
#define HAVE_DUMPS

#ifdef DEBUG_XML
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/xml         ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

int process_recursive(xmlDocPtr doc, char *xpath, int level, char *fn);

int process_data(xmlDocPtr doc, xmlNodePtr node, char *xpath, int level, char *fn) {
	char *newxpath = NULL;
	char *data = NULL;
	int size, i, found;

	node = node->xmlChildrenNode;
	while (node != NULL) {
		data = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);

		if ((data != NULL) && ((strlen(data) == 0) || (data[0] == 10))) {
			size = strlen(xpath) + strlen((const char *)node->name) + 2;
			newxpath = (char *)malloc( size * sizeof(char) );
			snprintf(newxpath, size, "%s/%s", xpath, node->name);

			process_recursive(doc, newxpath, level + 1, fn);
			free(newxpath);
		}
		else
		if (data != NULL) {
			found = 0;
			for (i = 0; i < xml_numAttr; i++) {
				if ((strcmp(xattr[i].name, (const char *)node->name) == 0)
					&& ((strcmp(xattr[i].node, xpath) == 0))
					&& ((strcmp(xattr[i].value, data) == 0))
					&& ((strcmp(xattr[i].filename, fn) == 0)))
					found = 1;
			}

			if (!found) {
				if (xattr == NULL)
					xattr = (tAttr *)malloc( sizeof(tAttr) );
				else
					xattr = (tAttr *)realloc( xattr, (xml_numAttr + 1) * sizeof(tAttr) );

				xattr[xml_numAttr].name = strdup( (const char *)node->name);
				xattr[xml_numAttr].node = strdup(xpath);
				xattr[xml_numAttr].value = strdup(data);
				xattr[xml_numAttr].filename = strdup(fn);
				xattr[xml_numAttr].numIter = xml_numIter;
				xml_numAttr++;
			}
		}

		free(data);

		node = node->next;
	}

	xml_numIter++;
	return 0;
}

int process_recursive(xmlDocPtr doc, char *xpath, int level, char *fn)
{
	int i, num;
	xmlXPathContextPtr context;
	xmlXPathObjectPtr op;
	xmlNodeSetPtr nodeset;

	context = xmlXPathNewContext(doc);
	if (context == NULL) {
		DPRINTF("Error in xmlXPathNewContext\n");
		return 0;
	}
	
	DPRINTF("Trying to access xPath node %s\n", xpath);
	op = xmlXPathEvalExpression( (xmlChar *)xpath, context);
	xmlXPathFreeContext(context);
	if (op == NULL) {
		DPRINTF("Error in xmlXPathEvalExpression\n");
		return -EIO;
	}
	if(xmlXPathNodeSetIsEmpty(op->nodesetval)){
		xmlXPathFreeObject(op);
		DPRINTF("No result\n");
		return -EINVAL;
	}
	
	nodeset = op->nodesetval;
	num = nodeset->nodeNr;
	
	for (i = 0; i < num; i++)
		process_data(doc, nodeset->nodeTab[i], xpath, level, fn);

	xmlXPathFreeObject(op);
	
	return 0;
}

int xml_init(void) {
	xml_numAttr = 0;
	xml_numIter = 0;

	xattr = NULL;
	return 0;
}

int xml_load(char *xmlFile) {
	xmlDocPtr doc;

	if (access(xmlFile, R_OK) != 0) {
		fprintf(stderr, "Error: File %s doesn't exist or is not accessible for reading.\n", xmlFile);
		return -ENOENT;
	}

	doc = xmlParseFile(xmlFile);

	process_recursive(doc, ROOT_ELEMENT_DEFS, 0, xmlFile);

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;
}

int xml_query(char *xmlFile, char *xPath) {
	xmlDocPtr doc;
	int ret = 0;

	if (access(xmlFile, R_OK) != 0) {
		fprintf(stderr, "Error: File %s doesn't exist or is not accessible for reading.\n", xmlFile);
		return -ENOENT;
	}

	doc = xmlParseFile(xmlFile);

	if (process_recursive(doc, xPath, 0, xmlFile) != 0)
		ret = -EINVAL;

	xmlFreeDoc(doc);
	xmlCleanupParser();

	return ret;
}

char *xml_get(char *basefile, char *node, char *name, char *value, char *out_value) {
	int i, myNumIter = -1;
	char *bn = NULL;
	char *fbn = NULL;

	bn = basename( strdup(basefile) );
	for (i = 0; i < xml_numAttr; i++) {
		fbn = basename( strdup(xattr[i].filename) );
		if (strcmp(bn, fbn) == 0) {
			if ((strcmp(xattr[i].node, node) == 0)
				&& (strcmp(xattr[i].name, name) == 0)
				&& (strcmp(xattr[i].value, value) == 0)) {
					myNumIter = xattr[i].numIter;
					break;
			}
		}
	}

	for (i = 0; i < xml_numAttr; i++) {
		if ((strcmp(xattr[i].node, node) == 0)
			&& (strcmp(xattr[i].name, out_value) == 0)
			&& (xattr[i].numIter == myNumIter)) {
				return strdup( xattr[i].value );
		}
	}

	return NULL;
}

void xml_dump(void) {
	int i;

	for (i = 0; i < xml_numAttr; i++) {
		dump_printf("Attribute #%d:\n", i + 1);
		dump_printf("\tFilename: %s\n", xattr[i].filename);
		dump_printf("\tNode: %s\n", xattr[i].node);
		dump_printf("\tName: %s\n", xattr[i].name);
		dump_printf("\tValue: %s\n", xattr[i].value);
	}
}

char **xml_get_all(char *nodename, int *oNum)
{
	int i, num = 0;
	char *node = NULL;
	char *name = NULL;
	char **ret = NULL;
	tTokenizer t;

	t = tokenize(nodename, ".");
	if (t.numTokens < 2)
		return ret;

	node = t.tokens[0];
	name = t.tokens[1];

	ret = malloc( sizeof(char *) );

	for (i = 0; i < xml_numAttr; i++) {
		if ((strcmp(xattr[i].node, node) == 0)
			&& (strcmp(xattr[i].name, name) == 0)) {
			ret = realloc( ret, (num + 1) * sizeof(char *) );

			ret[num] = strdup( xattr[i].value );
			num++;
		}
	}

	if (oNum != NULL)
		*oNum = num;

	free_tokens(t);
	return ret;
}

void xml_free_all(char **ret, int num)
{
	int i;

	for (i = 0; i < num; i++) {
		free(ret[i]);
		ret[i] = NULL;
	}

	free(ret);
}

int xml_cleanup(void) {
	int i;

	for (i = 0; i < xml_numAttr; i++) {
		if (xattr[i].node != NULL)
			free(xattr[i].node);

		if (xattr[i].name != NULL)
			free(xattr[i].name);

		if (xattr[i].value != NULL)
			free(xattr[i].value);

		if (xattr[i].filename != NULL)
			free(xattr[i].filename);

		xattr[i].node = NULL;
		xattr[i].name = NULL;
		xattr[i].value = NULL;
		xattr[i].filename = NULL;
	}

	//free(xattr);
	//xattr = NULL;

	return 0;
}

