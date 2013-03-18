#define DEBUG_TIMER

#ifdef USE_THREADS
#include "cdvws.h"

#ifdef DEBUG_TIMER
#define DPRINTF(fmt, ...) \
do { fprintf(stderr, "[cdv/timer       ] " fmt , ## __VA_ARGS__); } while (0)
#else
#define DPRINTF(fmt, ...) \
do {} while(0)
#endif

typedef struct tLoopData {
	tLooper *looper;
	int delay_before;
} tLoopData;

void *_loop_func(void *opaque)
{
	int delay_before;
	tLoopData *ld = (tLoopData *)opaque;

	tLooper *loop = ld->looper;
	delay_before = ld->delay_before;

	sleep(delay_before);

	while (1) {
		loop();

		usleep(500000);
	}

	pthread_exit(NULL);
}

void *_timer_func(void *opaque)
{
	tTimer *timer = (tTimer *)opaque;
	tLooper *looper;
	int seconds;

	seconds = timer->seconds;
	looper  = timer->looper;

	DPRINTF("%s: Starting timer function\n", __FUNCTION__);

	/* Loop forever */
	if (timer->remaining < 0) {
		while (timer->remaining != 0) {
			looper();

			sleep(seconds);
		}
	}
	else {
		while (!timer->remaining--) {
			looper();

			sleep(seconds);
		}
	}

	DPRINTF("%s: Done, exiting thread\n", __FUNCTION__); 
	pthread_exit(NULL);
}

int do_loop(tLooper looper, int delay_before)
{
	int ret = 0, rc = 0;
	pthread_t thread_id;
	tLoopData *ld = NULL;

	ld = (tLoopData *)malloc( sizeof(tLoopData) );
	ld->looper = looper;
	ld->delay_before = delay_before;
 
	rc = pthread_create(&thread_id, NULL, _loop_func, (void*) ld);
	if (rc) {
		DPRINTF("%s: Error creating looper thread\n", __FUNCTION__);
		ret = -EIO;
	}

	return ret;
}

tTimer *timer_start(tLooper looper, int seconds, int iterations, char *reason)
{
	int rc = 0;
	tTimer *timer = NULL;
	pthread_t thread_id;

	timer = (tTimer *)malloc( sizeof(tTimer ) );
	timer->looper = looper;
	timer->seconds = seconds;
	timer->remaining = iterations;
	memset(timer->reason, 0, sizeof(timer->reason));
	if (reason == NULL)
		strncpy(timer->reason, reason, sizeof(timer->reason));
 
	rc = pthread_create(&thread_id, NULL, _timer_func, (void*) timer); 
	if (rc) {
		DPRINTF("%s: Error creating timer thread\n", __FUNCTION__);
		free(timer);
		return NULL;
	}

	timer->thread_id = thread_id;

	return timer;
}

void timer_extend_iterations(tTimer *timer, int iterations, int overwrite)
{
	if (timer == NULL)
		return;

	if (overwrite == 0)
		timer->remaining += iterations;
	else
		timer->remaining = iterations;
}

void timer_destroy(tTimer *timer)
{
	if (timer == NULL)
		return;

	timer->remaining = 0;
	//pthread_cancel(timer->thread_id);
}
#else
int do_loop(tLooper looper)
{
	return -1;
}

tTimer *timer_start(tLooper looper, int seconds, int iterations, char *reason)
{
	return NULL;
}

void timer_extend_iterations(tTimer *timer, int iterations, int overwrite)
{
	DPRINTF("%s: Not supported\n", __FUNCTION__);
	return;
}

void timer_destroy(tTimer *timer)
{
	DPRINTF("%s: Not supported\n", __FUNCTION__);
	return;
}
#endif
