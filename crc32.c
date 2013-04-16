#define DEBUG_CRC

#include "cdvws.h"

#ifdef DEBUG_CRC
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/crc32       ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) do {} while (0)
#endif

void crc32_init()
{
	int i, j;
	unsigned long crc;
	unsigned long long poly;

	memset(crc_tab, 0, sizeof(crc_tab));
	DPRINTF("%s: Generating table\n", __FUNCTION__);

	poly = 0xEDB88320L;
	for (i = 0; i < 256; i++)
	{
		crc = i;
		for (j = 8; j > 0; j--)
		{
		if (crc & 1)
			crc = (crc >> 1) ^ poly;
		else
			crc >>= 1;
		}
		crc_tab[i] = crc;
	}
}

uint32_t crc32_block(unsigned char *block, uint32_t length, uint64_t initVal)
{
	register unsigned long crc;
	unsigned long i;

	crc = initVal;
	for (i = 0; i < length; i++)
		crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_tab[(crc ^ *block++) & 0xFF];

	return crc;
}

uint32_t crc32_file(char *filename, int chunkSize)
{
	int fd;
	long size;
	unsigned char *data = NULL;
	uint32_t ret = 0;
	int rc;

	fd = open(filename, O_RDONLY);
	if (fd <= 0)
		return 0;

        if (chunkSize < 0) {
		size = lseek(fd, 0, SEEK_END);
		lseek(fd, 0, SEEK_SET);
		DPRINTF("%s: Got %s size: 0x%" PRIx32" bytes\n",
			__FUNCTION__, filename, size);
	}
	else {
		size = chunkSize;
		DPRINTF("%s: Getting %s CRC by 0x%"PRIx32" bytes\n",
			__FUNCTION__, filename, size);
	}

	data = malloc( (size + 1) * sizeof(unsigned char) );
	if (!data) {
		DPRINTF("%s: Cannot allocate memory\n", __FUNCTION__);
		return 0;
	}

	/* CRC Initial value */
	ret = 0xFFFFFFFF;
	while ((rc = read(fd, data, size)) > 0) {
                ret = crc32_block(data, rc, ret);
		memset(data, 0, size);
	}
	close(fd);
	free(data);

	DPRINTF("%s: CRC-32 value for %s is 0x%"PRIx32"\n", __FUNCTION__,
		filename, ret ^ 0xFFFFFFFF);

	return ret ^ 0xFFFFFFFF;
}

