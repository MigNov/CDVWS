#include "cdvws.h"

#ifdef DEBUG_MINCRYPT
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/w-mincrypt  ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

#ifdef USE_MINCRYPT
long wrap_mincrypt_get_version(void)
{
	return mincrypt_get_version();
}

int wrap_mincrypt_set_password(char *salt, char *password)
{
	mincrypt_set_password(salt, password, -1);

	return 0;
}

unsigned char *wrap_mincrypt_encrypt(unsigned char *block, size_t size, int id, size_t *new_size)
{
	return mincrypt_encrypt(block, size, id, new_size);
}

unsigned char *wrap_mincrypt_decrypt(unsigned char *block, size_t size, int id, size_t *new_size, int *read_size)
{
	return mincrypt_decrypt(block, size, id, new_size, read_size);
}

int wrap_mincrypt_set_encoding_type(int type)
{
	return mincrypt_set_encoding_type(type);
}

unsigned char *wrap_mincrypt_base64_encode(unsigned char *in, size_t *size)
{
	return mincrypt_base64_encode((const char *)in, size);
}

unsigned char *wrap_mincrypt_base64_decode(unsigned char *in, size_t *size)
{
	return mincrypt_base64_decode((const char *)in, size);
}

void wrap_mincrypt_cleanup(void)
{
	mincrypt_cleanup();
}
#else
long wrap_mincrypt_get_version(void) { return -1; }
int wrap_mincrypt_set_password(char *salt, char *password) { return -1; }
unsigned char *wrap_mincrypt_encrypt(unsigned char *block, size_t size, int id, size_t *new_size) { return NULL; }
unsigned char *wrap_mincrypt_decrypt(unsigned char *block, size_t size, int id, size_t *new_size, int *read_size)
	{ return NULL; }
int wrap_mincrypt_set_encoding_type(int type) { return -1; }
unsigned char *wrap_mincrypt_base64_encode(unsigned char *in, size_t *size) { return NULL; }
unsigned char *wrap_mincrypt_base64_decode(unsigned char *in, size_t *size) { return NULL; }
void wrap_mincrypt_cleanup(void) {};
#endif
