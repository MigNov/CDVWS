#define DEBUG_NOTIFIER

#ifdef USE_NOTIFIER
#include "cdvws.h"

#ifdef DEBUG_NOTIFIER
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/notifier    ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

uint32_t _notifier_checksum_for_file(tNotifier *notifier, char *path, char *filename);
int _notifier_checksum_set_for_file(tNotifier *notifier, char *path, char *filename, uint32_t newval);

void *_inotify_loop(void *opaque)
{
	tNotifier *notifier;
	struct timeval timeout;
	fd_set read_fds;
	int fd, wd, i, result;
	char buf[INOTIFY_BUFFER_SIZE];

	if (opaque == NULL)
		return NULL;

	notifier = (tNotifier *)opaque;
	fd = notifier->fd;
	wd = notifier->wd;

	result = 0;
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;
	FD_ZERO(&read_fds);
	FD_SET(fd, &read_fds);

	DPRINTF("%s: Entering iNotify loop (TID 0x%x)\n", __FUNCTION__, pthread_self());

	while ( !notifier->done ) {
		select(fd+1, &read_fds, (fd_set *)0, (fd_set *)0, &timeout);
		result = read(fd, buf, INOTIFY_BUFFER_SIZE);
		if (result > 0) {
			i = 0;
			while ( i < result ) {
				struct inotify_event *event = (struct inotify_event *)&buf[i];

				if ((strlen(event->name) > 0) && (event->name[0] != '.')
					&& (strstr(event->name, ".lock") == NULL)) {
					DPRINTF("Filesystem change found: %s (create: %s, modify: %s, delete: %s )\n",
						event->name,
						event->mask & IN_CREATE ? "True" : "False",
						event->mask & IN_MODIFY ? "True" : "False",
						event->mask & IN_DELETE ? "True" : "False");

					if (notifier->callback != NULL) {
						int j, mask = 0;

						if (event->mask & IN_CREATE)
							mask |= NOTIFY_CREATE;
						if (event->mask & IN_MODIFY)
							mask |= NOTIFY_MODIFY;
						if (event->mask & IN_DELETE)
							mask |= NOTIFY_DELETE;

						for (j = 0; j < CDV_ALLOC_INDEX(_notifiers) + 1; j++) {
							if ((_notifiers[j].fd == fd)
								&& (_notifiers[j].wd == wd)
								&& (_notifiers[j].thread_id > 0)) {
								char path[4096] = { 0 };
								snprintf(path, sizeof(path), "%s/%s", notifier->path, event->name);
								while (lock_file_for_file(path, FLOCK_EXISTS | LOCK_WRITE) == 0) {
									usleep(50000);

									if (crc32_file(path, -1) != 0)
										break;
								}

								uint32_t cs = crc32_file(path, -1);
								if (_notifier_checksum_for_file(notifier, path, event->name) != cs) {
									notifier->callback(j, event->name, mask);
									if (_notifier_checksum_set_for_file(notifier, path,
										event->name, cs) == 0)
										DPRINTF("%s: Checksum for %s updated to 0x%"PRIx32"\n",
											__FUNCTION__, event->name, cs);
								}
							}
						}
					}
				}

				i += event->len + INOTIFY_EVENT_SIZE;
			}
		}
	}

	DPRINTF("%s: Exiting iNotify loop (TID 0x%x)\n", __FUNCTION__, pthread_self());

	return NULL;
}

void notifier_pool_add(tNotifier *notifier)
{
	if (notifier == NULL)
		return;

	CDV_ALLOC(_notifiers, struct tNotifier);
	if (CDV_ALLOC_ERROR()) {
		DPRINTF("CDV Error: %d\n", CDV_ALLOC_ERROR());
		return;
	}

	_notifiers[ CDV_ALLOC_INDEX(_notifiers) ] = *notifier;
}

void notifier_pool_dump(void)
{
	int i;

	DPRINTF("Notifier pool dump\n");
	DPRINTF("==================\n");
	for (i = 0; i < CDV_ALLOC_INDEX(_notifiers) + 1; i++) {
		DPRINTF("\tNotifier #%d:\n", i + 1);
		DPRINTF("\t\tRunning: %s\n", (_notifiers[i].thread_id == 0) ? "False" : "True");
		DPRINTF("\t\tPath: %s\n", _notifiers[i].path);
		DPRINTF("\t\t(FD: %d)\n", _notifiers[i].fd);
		DPRINTF("\t\t(WD: %d)\n", _notifiers[i].wd);
		DPRINTF("\t\t(Done: %d)\n", _notifiers[i].done);
		DPRINTF("\t\t(Thread_id: 0x%x)\n", _notifiers[i].thread_id);
		DPRINTF("\t\t(Callback: %p)\n", _notifiers[i].callback);
		if ((_notifiers[i].checksums != NULL) && (_notifiers[i].checksums->count)) {
			DPRINTF("\t\t(Checksums:%c", '\n');
			int j, cnt = _notifiers[i].checksums->count;
			for (j = 0; j < cnt; j++) {
				tFileChecksum *cs = (tFileChecksum *)_notifiers[i].checksums->items[j];

				DPRINTF("\t\tFile #%d:\n", j + 1);
				DPRINTF("\t\t\tFilename: %s\n", cs->filename);
				DPRINTF("\t\t\tCRC-32: 0x%"PRIx32"\n", cs->checksum);
				DPRINTF("%c", '\n');
			}
			
		}
		DPRINTF("%c\n", ' ');
	}

	if (CDV_ALLOC_INDEX(_notifiers) + 1 == 0)
		DPRINTF("No notifier in pool\n");

	DPRINTF("==================\n");
}

void notifier_pool_free(void)
{
	int i;

	for (i = 0; i < CDV_ALLOC_INDEX(_notifiers) + 1; i++) {
		if (_notifiers[i].thread_id > 0)
			notifier_stop( &_notifiers[i] );

		notifier_destroy_nofree( &_notifiers[i] );
	}

	DPRINTF("%s: Done\n", __FUNCTION__);
	_notifiers_count = 0;
	free(_notifiers);
	_notifiers = NULL;
}

uint32_t _notifier_checksum_for_file(tNotifier *notifier, char *path, char *filename)
{
	int i;

	if ((notifier == NULL) || (notifier->checksums == NULL))
		return 0;

	for (i = 0; i < notifier->checksums->count; i++) {
		tFileChecksum *cso = (tFileChecksum *)notifier->checksums->items[i];
		if (strcmp(filename, cso->filename) == 0)
			return cso->checksum;
	}

	return 0;
}

int _notifier_checksum_set_for_file(tNotifier *notifier, char *path, char *filename, uint32_t newval)
{
	int i;

	if ((notifier == NULL) || (notifier->checksums == NULL))
		return -EINVAL;

	for (i = 0; i < notifier->checksums->count; i++) {
		tFileChecksum *cso = (tFileChecksum *)notifier->checksums->items[i];
		if (strcmp(filename, cso->filename) == 0) {
			((tFileChecksum *)notifier->checksums->items[i])->checksum = newval;
			return 0;
		}
	}

	return -ENOENT;
}

void *_notifier_checksums(char *path, char *filename)
{
	char tmp[4096] = { 0 };
	tFileChecksum *ret = NULL;

	snprintf(tmp, sizeof(tmp), "%s/%s", path, filename);

	ret = malloc( sizeof(tFileChecksum) );
	ret->filename = strdup(filename);
	ret->type = FCHECKSUM_CRC32;
	ret->checksum = crc32_file(tmp, -1);

	return (void *)ret;
}

void *apply_to_all_files(char *dir, int size, tTwoStrFuncPtr func)
{
	tCDVList *ret = NULL;
	void *tmp = NULL;
	struct dirent *de = NULL;
	DIR *d = opendir(dir);
	int num = 0;

	if (d == NULL)
		return NULL;

	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] != '.')
			num++;
	}

	if (size > 0xFF)
		size -= 0xFF;
	else {
		if (size == CDVLIST_TYPE_CHECKSUMS)
			size = sizeof(tFileChecksum);
	}

	ret = malloc( sizeof(tCDVList) );
	ret->items = malloc( size * num );
	ret->count = num;
	num = 0;

	rewinddir(d);
	while ((de = readdir(d)) != NULL) {
		if (de->d_name[0] != '.')
			ret->items[num++] = func(dir, de->d_name);
	}

	closedir(d);

	return (void *)ret;
}

/* Parameter notifier can be NULL, otherwise inherit fd from notifier */
tNotifier *notifier_create(tNotifier *notifier, char *path, int changes_only, tNotifyCallback cb)
{
	tNotifier *ret = NULL;
	int flags = IN_CREATE | IN_MODIFY | IN_DELETE;

	if (changes_only)
		flags = IN_MODIFY;

	ret = (tNotifier *)malloc( sizeof(tNotifier) );
	if (notifier != NULL)
		ret->fd = notifier->fd;
	else
		ret->fd = inotify_init();

	ret->done = 0;
	ret->callback = cb;
	ret->path = strdup(path);
	ret->wd = inotify_add_watch(ret->fd, path, flags);
	ret->thread_id = 0;
	ret->checksums = apply_to_all_files(path, CDVLIST_TYPE_CHECKSUMS, _notifier_checksums);

	return ret;
}

int notifier_start(tNotifier *notifier)
{
	int rc, ret = 0;
	pthread_t thread_id;

	if (notifier == NULL) {
		DPRINTF("%s: Notifier is NULL pointer\n", __FUNCTION__);
		return -1;
	}

	if (notifier->thread_id != 0) {
		DPRINTF("%s: Notifier seems to be already started\n", __FUNCTION__);
		return -1;
	}

	notifier->done = 0;

	rc = pthread_create(&thread_id, NULL, _inotify_loop, (void *)notifier);
	if (rc) {
		DPRINTF("%s: Error creating notifier thread\n", __FUNCTION__);
		ret = -EIO;
	}

	notifier->thread_id = thread_id;
	DPRINTF("%s: Thread with TID 0x%x started\n", __FUNCTION__, thread_id);
	return ret;
}

void notifier_stop(tNotifier *notifier)
{
	if (notifier == NULL)
		return;

	if (notifier->thread_id == 0)
		return;

	notifier->done = 1;

	pthread_cancel(notifier->thread_id);
	DPRINTF("%s: Thread with TID 0x%x stopped\n", __FUNCTION__, notifier->thread_id);
	notifier->thread_id = 0;
}

void notifier_destroy_nofree(tNotifier *notifier)
{
	if (notifier == NULL)
		return;

	inotify_rm_watch(notifier->fd, notifier->wd);

	if (notifier->thread_id > 0)
		pthread_cancel(notifier->thread_id);
	if (notifier->checksums != NULL) {
		int i, num = notifier->checksums->count;

		for (i = 0; i < num; i++) {
			free(notifier->checksums->items[i]);
			notifier->checksums->items[i] = NULL;
		}

		free(notifier->checksums);
		notifier->checksums = NULL;
	}

	notifier->done = 1;
	free(notifier->path);
	notifier->path = NULL;
}

void notifier_destroy(tNotifier *notifier)
{
	if (notifier == NULL)
		return;

	notifier_destroy_nofree(notifier);

	free(notifier);
	notifier = NULL;
}
#else
tNotifier *notifier_create(tNotifier *notifier, char *path, int changes_only) { return NULL; }
int notifier_start(tNotifier *notifier) { return -1; };
void notifier_stop(tNotifier *notifier) {};
void notifier_destroy(tNotifier *notifier) {};
void notifier_pool_add(tNotifier *notifier) {};
void notifier_pool_dump(void) {};
void notifier_pool_free(void) {};
#endif
