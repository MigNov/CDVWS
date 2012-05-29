#define DEBUG_SHELL
#include "cdvws.h"

#ifdef DEBUG_SHELL
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/cdv-shell   ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

char *get_string(char *prompt)
{
	int c;
	char a[2] = { 0 };
	char buf[1024] = { 0 };

	printf("%s", prompt);
	while ((c = getchar()) != '\n') {
		a[0] = c;

		strcat(buf, a);
	}

	return strdup( buf );
}

int run_shell(void)
{
	while (1) {
		char *str = get_string("(cdv) ");

		if (str == NULL)
			return -EIO;

		if (strcmp(str, "\\q") == 0)
			break;

		if (str[0] == -1)
			break;

		DPRINTF("%s: You wrote '%s'\n", __FUNCTION__, str);
	}

	return 0;
}
