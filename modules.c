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
	typedef void (*tProcess)(void);
	void *process2 = NULL;
	void *lib = NULL;
	tProcess proc;

	if (access(libname, F_OK) != 0) {
		DPRINTF("%s: Cannot find module '%s'\n", __FUNCTION__, libname);
		return -ENOENT;
	}

	DPRINTF("%s: Loading module '%s'\n", __FUNCTION__, libname);

	lib = dlopen(libname, RTLD_LAZY);
	if (lib == NULL) {
		DPRINTF("%s: Cannot load library '%s'", __FUNCTION__, libname);
		return -ENOENT;
	}

	process2 = dlsym(lib, "cdvws_process");
	if (process2 == NULL) {
		DPRINTF("%s: Cannot read processor symbol from library %s",
			__FUNCTION__, libname);
		goto cleanup;
	}

	proc = (tProcess) process2;

	proc();
cleanup:
	dlclose(lib);
	return 0;
}

