#define DEBUG_MODULES

#include "cdvws.h"

#ifdef DEBUG_MODULES
#define DPRINTF(fmt, args...) \
do { fprintf(stderr, "[cdv/modules     ] " fmt, args); } while (0)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

int modules_initialize(void)
{
	char buf[BUFSIZE] = { 0 };

	/* Initialize the module directory path */
	getcwd(buf, sizeof(buf));
	strcat(buf, "/modules");

	if (access(buf, X_OK) != 0)
		return -ENOENT;

	_modules_directory = strdup(buf);
	return 0;
}

char *modules_get_directory(void)
{
	return strdup(_modules_directory);
}

int module_load(char *libname)
{
	if (access(libname, F_OK) != 0) {
		DPRINTF("%s: Cannot find module '%s'\n", __FUNCTION__, libname);
		return -ENOENT;
	}

	DPRINTF("%s: Loading module '%s'\n", __FUNCTION__, libname);

/*
	void *lib = NULL;
	typedef int (*tIsApplicableFunc) (void);
	void *pIsApplicable = NULL;

	lib = dlopen(libname, RTLD_LAZY);
	if (lib == NULL) {
		DPRINTF("%s: Cannot load library '%s'", __FUNCTION__, libname);
		return -ENOENT;
	}

	pIsApplicable = dlsym(lib, "cdv_is_applicable");
	if (pIsApplicable == NULL) {
		DPRINTF("%s: Cannot read 'is applicable' symbol from library %s",
			__FUNCTION__, libname);
		goto cleanup;
	}
	tIsApplicableFunc fIsApplicable = (tIsApplicableFunc) pIsApplicable;
	if (fIsApplicable == NULL)
		goto cleanup;

cleanup:
	dlclose(lib);
*/
	return 0;
}

