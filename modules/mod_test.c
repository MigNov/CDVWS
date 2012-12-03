#define DEBUG_MOD_TEST

#include "../modules.h"

#ifdef DEBUG_MOD_TEST
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/mod_test    ] " fmt , ##args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

void cdvws_process() {
	char *v = NULL;

	v = variable_get_element_as_string("module_var", NULL);
	if (v != NULL) {
		DPRINTF("We have module var set to '%s'\n", v);

		variable_add("module_var", v, TYPE_MODULE, 0, TYPE_STRING);
	}
	else {
		DPRINTF("We don't have module var set, setting to \"unset\"\n");

		variable_add("module_var", "unset", TYPE_MODULE, 0, TYPE_STRING);
	}

	v = utils_free("mod_test.v", v);
}

