#define DEBUG_DEFS

#include "cdvws.h"

#ifdef DEBUG_DEFS
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/definitions ] " fmt , args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int definitions_initialize(void)
{
	return 0;
}

void definitions_cleanup(void)
{
	/* TODO: Cleanup the definitions if applicable */
}

int definitions_load_directory(char *dir)
{
	DPRINTF("%s: Loading definitions directory '%s'\n", __FUNCTION__, dir);

	if (access(dir, X_OK) != 0) {
		DPRINTF("%s: Cannot open definitions directory '%s'\n", __FUNCTION__, dir);
		return -ENOENT;
	}

	/* TODO: Handle files and save to defs structure for further processing */

	return 0;
}

