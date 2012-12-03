#ifdef USE_PCRE

#include "cdvws.h"

#ifdef DEBUG_REGEX
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/regex       ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

typedef struct tRegExs {
	char *expr;
	char *tran;
} tRegExs;

tRegExs *_tregexs;
int _tregexs_num;

int _regex_match(char *regex, char *str, char **matches, int *match_count)
{
	const char	*error;
	int		erroffset;
	pcre		*re;
	int		rc;
	int		i, nmatches = 0;
	int		ovector[100];
	char		*data = NULL;
	char		tmp[4096] = { 0 };

	DPRINTF("%s: Trying to match '%s' in '%s'\n", __FUNCTION__, regex, str);

	re = pcre_compile (regex,          /* the pattern */
			PCRE_MULTILINE,
			&error,         /* for error message */
			&erroffset,     /* for error offset */
			0);             /* use default character tables */

	if (!re)
		return -1;

	unsigned int offset = 0;
	unsigned int len    = strlen(str);
	while (offset < len && (rc = pcre_exec(re, 0, str, len, offset, 0, ovector, sizeof(ovector))) >= 0)
	{
		for(i = 0; i < rc; ++i)
		{
			snprintf(tmp, sizeof(tmp), "%.*s", ovector[2*i+1] - ovector[2*i], str + ovector[2*i]);
			data = strdup( str + ovector[2*i] );
			if (nmatches > 0) {
				snprintf(tmp, sizeof(tmp), "%.*s", ovector[2*i+1] - ovector[2*i], str + ovector[2*i]);

				if (match_count != NULL) {
					matches = realloc( matches, nmatches * sizeof(char *));
					matches[ nmatches - 1 ] = strdup( tmp );
				}
			}
			//DPRINTF("%s: Match #%2d: %.*s\n", __FUNCTION__, i, ovector[2*i+1] - ovector[2*i], str + ovector[2*i]);
			nmatches++;
			data = utils_free("regex._regex_match.data", data);
		}
		offset = ovector[1];
	}

	if (match_count != NULL)
		*match_count = nmatches - 1;

	return (nmatches > 0);
}

int regex_exists(char *expr)
{
	int i;

	if (_tregexs_num == 0)
		return 0;

	for (i = 0; i < _tregexs_num; i++) {
		if (strcmp(_tregexs[i].expr, expr) == 0)
			return 1;
	}

	return 0;
}

char *regex_get(char *expr)
{
	int i;

	if (_tregexs_num == 0)
		return NULL;

	for (i = 0; i < _tregexs_num; i++) {
		if (strcmp(_tregexs[i].expr, expr) == 0)
			return strdup( _tregexs[i].tran );
	}

	return NULL;
}

int regex_match(char *expr, char *str)
{
	return (_regex_match(expr, str, NULL, NULL) == 1) ? 1 : 0;
}

int regex_parse(char *xmlFile)
{
	xmlDocPtr doc;
	xmlXPathContextPtr context;
	xmlXPathObjectPtr result;
	xmlNodeSetPtr nodeset;
        xmlNodePtr node;
	char *match;
	char *data;
	int i;

	if (access(xmlFile, R_OK) != 0) {
		fprintf(stderr, "Error: File %s doesn't exist or is not accessible for reading.\n", xmlFile);
		return -ENOENT;
	}

	doc = xmlParseFile(xmlFile);
	if (!doc) {
		DPRINTF("Cannot get xmlDocPtr\n");
		return -EINVAL;
	}

	context = xmlXPathNewContext(doc);
	if (!context) {
		DPRINTF("Cannot get new XPath context\n");
		return -EIO;
	}

	result = xmlXPathEvalExpression( (xmlChar *)"//rewrite-rules/rule", context);
	nodeset = result->nodesetval;

	for (i = 0; i < nodeset->nodeNr; i++) {
                node = nodeset->nodeTab[i];
		while (node != NULL) {
	        	data = (char *)xmlNodeListGetString(doc, node->xmlChildrenNode, 1);
			if (data != NULL) {
				match = (char *)xmlGetProp(node, (xmlChar *)"match");

				if (regex_exists(match) == 0) {
					if (_tregexs == NULL) {
						_tregexs = (tRegExs *)utils_alloc( "regex.regex_parse._tregexs", sizeof(tRegExs) );
						_tregexs_num = 0;
					}
					else
						_tregexs = (tRegExs *)realloc( _tregexs, (_tregexs_num + 1) * sizeof(tRegExs) );

					_tregexs[_tregexs_num].expr = strdup( match );
					_tregexs[_tregexs_num].tran = strdup( data );
					_tregexs_num++;
				}

				match = utils_free("regex.regex_parse.match", match);
			}

			data = utils_free("regex.regex_parse.data", data);

			node = node->next;
		}
	}

	xmlXPathFreeObject(result);
	xmlFreeDoc(doc);
	xmlCleanupParser();

	return 0;
}

void regex_dump(void)
{
	int i;

	if (_tregexs_num == 0)
		return;

	DPRINTF("Regular Expressions Dump\n");
	for (i = 0; i < _tregexs_num; i++) {
		DPRINTF("RegEx #%d:\n", i + 1);
		DPRINTF("\tExpression: %s\n", _tregexs[i].expr);
		DPRINTF("\tTranslation: %s\n", _tregexs[i].tran);
	}
}

void regex_free(void)
{
	int i;

	for (i = 0; i < _tregexs_num; i++) {
		_tregexs[i].tran = utils_free("regex.regex_free._tregexs[].tran", _tregexs[i].tran);
		_tregexs[i].expr = utils_free("regex.regex_free._tregexs[].expr", _tregexs[i].expr);
	}

	if (_tregexs_num > 0)
		DPRINTF("%s: %d regular expression entries freed\n",
			__FUNCTION__, _tregexs_num);

	_tregexs_num = 0;
}

int regex_get_idx(char *str)
{
	int i;

	if (_tregexs_num == 0)
		return -1;

	for (i = 0; i < _tregexs_num; i++) {
		if (_regex_match(_tregexs[i].expr, str, NULL, NULL))
			return i;
	}

	return -1;
}

char **regex_get_matches(char *str, int *num_matches)
{
	int i;
	int matchcnt;
	char **matches = NULL;

	if ((_tregexs_num == 0) || (num_matches == NULL))
		return NULL;

	matches = (char **)utils_alloc( "regex.regex_get_matches.matches", sizeof(char *) );
	for (i = 0; i < _tregexs_num; i++) {
		if (_regex_match(_tregexs[i].expr, str, matches, &matchcnt)) {
			*num_matches = matchcnt;
			return matches;
		}
	}

	return NULL;
}

char *regex_format_new_string(char *str)
{
	int num_matches = 0;
	char **matches = NULL;
	char t1[16] = { 0 };
	char *tmp = NULL;
	int i;

	matches = regex_get_matches(str, &num_matches);

	i = regex_get_idx(str);
	if (i < 0) {
		DPRINTF("%s: Cannot find RegEx to match %s\n", __FUNCTION__, str);
		return NULL;
	}

	tmp = strdup( _tregexs[i].tran );
	for (i = 0; i < num_matches; i++) {
		snprintf(t1, sizeof(t1), "$%d", i + 1);

		tmp = replace(tmp, t1, matches[i]);
	}

	regex_free_matches(matches, num_matches);

	return tmp;
}

void regex_dump_matches(char **elements, int num_elems)
{
	int i;

	if ((num_elems <= 0) || (elements == NULL))
		return;

	DPRINTF("Regular Expression Matched Elements Dump:\n");
	for (i = 0; i < num_elems; i++) {
		DPRINTF("\t$%d: %s\n", i + 1, elements[i]);
	}
}

void regex_free_matches(char **elements, int num_elems)
{
	int i;

	for (i = 0; i < num_elems; i++)
		elements[i] = utils_free("regex.regex_free_matches.elements[]", elements[i]);

	if (num_elems > 0)
		DPRINTF("%s: %d elements freed\n", __FUNCTION__, num_elems);

	elements = NULL;
	num_elems = 0;
}
#else
int regex_parse(char *xmlFile) { return -1; }
void regex_dump(void) {};
void regex_free(void) {};
int regex_get_idx(char *str) { return -1; };
char **regex_get_matches(char *str, int *num_matches) { return NULL; };
void regex_dump_matches(char **elements, int num_elems) { };
void regex_free_matches(char **elements, int num_elems) { };
char *regex_format_new_string(char *str) { return NULL; };
int regex_match(char *expr, char *str) { return -1; }
#endif
