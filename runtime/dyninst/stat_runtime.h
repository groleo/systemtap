/* -*- linux-c -*- 
 * Stat Runtime Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_STAT_RUNTIME_H_
#define _STAPDYN_STAT_RUNTIME_H_

#include <pthread.h>
#include <unistd.h>
#include <sched.h>

#ifdef NEED_STAT_LOCKS
#define STAT_LOCK(sd)		pthread_mutex_lock(&(sd)->lock)
#define STAT_UNLOCK(sd)		pthread_mutex_unlock(&(sd)->lock)
#else
#define STAT_LOCK(sd)		do {} while (0)
#define STAT_UNLOCK(sd)		do {} while (0)
#endif

static int STAT_GET_CPU(void)
{
	return _stp_runtime_get_data_index();
}

#define STAT_PUT_CPU()	do {} while (0)


#define _stp_stat_per_cpu_ptr(stat, cpu) \
	((stat_data *)((void *)((stat)->sd) + ((stat)->size * (cpu))))


static int _stp_stat_initialize_locks(Stat st)
{
#ifdef NEED_STAT_LOCKS
	int i, rc;
	for_each_possible_cpu(i) {
		stat_data *sdp = _stp_stat_per_cpu_ptr (st, i);

		if ((rc = pthread_mutex_init(&sdp->lock, NULL)) != 0) {
			int j;

			_stp_error("Couldn't initialize stat mutex: %d\n", rc);
			return rc;
		}
	}
	if ((rc = pthread_mutex_init(&st->agg->lock, NULL)) != 0) {
		_stp_error("Couldn't initialize stat mutex: %d\n", rc);
	}
	return rc;
#else
	return 0;
#endif
}


static void _stp_stat_destroy_locks(Stat st)
{
#ifdef NEED_STAT_LOCKS
	int i;
	for_each_possible_cpu(i) {
		stat_data *sdp = _stp_stat_per_cpu_ptr(st, i);
		(void)pthread_mutex_destroy(&sdp->lock);
	}
	(void)pthread_mutex_destroy(&st->agg->lock);
#endif
}

#endif /* _STAPDYN_STAT_RUNTIME_H_ */
