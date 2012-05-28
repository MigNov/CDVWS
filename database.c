#define DEBUG_DB

#include "cdvws.h"
//#include <libxml/parser.h>
//#include <libxml/xpath.h>

#ifdef DEBUG_DB
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/database    ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

char *database_format_query(char *xmlFile, char *table, char *type)
{
	int i, got_condition;
	char path[1024] = { 0 };
	char query[BUFSIZE] = { 0 };

	snprintf(path, sizeof(path), "//page-definition/queries/query[@table=\"%s\"]/%s", table, type);

	//xml_cleanup();
	xml_init();

	if (xml_query(xmlFile, path) != 0)
		return NULL;

	if (strcmp(type, "select") == 0) {
		char *bn = NULL;
		char *val = NULL;

		strcpy(query, "SELECT ");

		for (i = 0; i < xml_numAttr; i++) {
			if (strcmp(xattr[i].name, "field") == 0) {
				strcat(query, xattr[i].value);
				strcat(query, ",");
			}
		}

		strcat(query, " FROM ");
		strcat(query, table);

		got_condition = 0;
		for (i = 0; i < xml_numAttr; i++) {
			if ((strcmp(xattr[i].name, "name") == 0)
				&& (strstr(xattr[i].node, "/conditions") != NULL)) {
				bn = strdup( xattr[i].filename );

				if (!got_condition) {
					strcat(query, " WHERE ");
					got_condition = 1;
				}

				strcat(query, xattr[i].value);
				strcat(query, " = \"");

				val = xml_get(bn, xattr[i].node, "name", xattr[i].value, "value");

				if (val == NULL) {
					fprintf(stderr, "Error: Cannot get value\n");
					return NULL;
				}
				strcat(query, val);
			}
		}
	}
	else
	if (strcmp(type, "insert") == 0) {
		snprintf(query, sizeof(query), "INSERT INTO %s(", table);
		for (i = 0; i < xml_numAttr; i++) {
			if (strcmp(xattr[i].name, "name") == 0) {
				strcat(query, xattr[i].value);
				strcat(query, ", ");
			}
		}

		query[strlen(query) - 2] = ' ';

		strcat(query, ") VALUES(");
		for (i = 0; i < xml_numAttr; i++) {
			if (strcmp(xattr[i].name, "value") == 0) {
				strcat(query, "\"");
				strcat(query, xattr[i].value);
				strcat(query, "\"");
				strcat(query, ", ");
			}
		}
		query[strlen(query) - 2] = ' ';
		strcat(query, ")");
	}
	else
	if (strcmp(type, "update") == 0) {
		char *bn = NULL;
		char *val = NULL;

		snprintf(query, sizeof(query), "UPDATE %s SET ", table);
		for (i = 0; i < xml_numAttr; i++) {
			if ((strcmp(xattr[i].name, "name") == 0)
				&& (strstr(xattr[i].node, "/fields") != NULL)) {
				bn = strdup( xattr[i].filename );
				strcat(query, xattr[i].value);
				strcat(query, " = \"");

				val = xml_get(bn, xattr[i].node, "name", xattr[i].value, "value");
				if (val == NULL) {
					fprintf(stderr, "Error: Cannot get value\n");
					return NULL;
				}
				strcat(query, val);
				strcat(query, "\", ");
			}
		}

		query[strlen(query) - 2] = ' ';

		strcat(query, " WHERE ");

		for (i = 0; i < xml_numAttr; i++) {
			if ((strcmp(xattr[i].name, "name") == 0)
				&& (strstr(xattr[i].node, "/conditions") != NULL)) {
				bn = strdup( xattr[i].filename );

				strcat(query, xattr[i].value);
				strcat(query, " = \"");

				val = xml_get(bn, xattr[i].node, "name", xattr[i].value, "value");

				if (val == NULL) {
					fprintf(stderr, "Error: Cannot get value\n");
					return NULL;
				}
				strcat(query, val);
				strcat(query, "\"");
			}
		}
	}
	else
	if (strcmp(type, "delete") == 0) {
		char *bn = NULL;
		char *val = NULL;

		snprintf(query, sizeof(query), "DELETE FROM %s WHERE ", table);

		for (i = 0; i < xml_numAttr; i++) {
			if ((strcmp(xattr[i].name, "name") == 0)
				&& (strstr(xattr[i].node, "/conditions") != NULL)) {
				bn = strdup( xattr[i].filename );

				strcat(query, xattr[i].value);
				strcat(query, " = \"");

				val = xml_get(bn, xattr[i].node, "name", xattr[i].value, "value");

				if (val == NULL) {
					fprintf(stderr, "Error: Cannot get value\n");
					return NULL;
				}
				strcat(query, val);
				strcat(query, "\"");
			}
		}
	}

	xml_cleanup();

	return strdup(query);
}

