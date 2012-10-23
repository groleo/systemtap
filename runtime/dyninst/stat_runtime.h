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

/* For dyninst, NEED_STAT_LOCKS is always on since we don't have real
   per-cpu data. */
#ifndef NEED_STAT_LOCKS
#define NEED_STAT_LOCKS
#endif

#define STAT_LOCK(sd)	pthread_mutex_lock(&(sd)->lock)
#define STAT_UNLOCK(sd)	pthread_mutex_unlock(&(sd)->lock)


/* Number of items allocated for a map or stat. Gets initialized to
   the number of online cpus. */
static inline int _stp_stat_get_cpus(void)
{
	static int online_cpus = 0;
	if (unlikely(online_cpus == 0)) {
		online_cpus = sysconf(_SC_NPROCESSORS_ONLN);
	}
	return online_cpus;
}


static inline int STAT_GET_CPU(void)
{
	/*
	 * Make sure the cpu number is within the range of
	 * [0.._stp_stat_get_cpus()]. If sched_getcpu() fails,
	 * it returns -1.
	 */
	int cpu = sched_getcpu() % _stp_stat_get_cpus();
	if (unlikely(cpu < 0))
		cpu = 0;
	return cpu;
}


#define STAT_PUT_CPU()	do {} while (0)


#define _stp_stat_for_each_cpu(cpu) \
	for ((cpu) = 0; (cpu) < _stp_stat_get_cpus(); (cpu)++)


#define _stp_stat_per_cpu_ptr(stat, cpu) \
	((stat_data *)((void *)((stat)->sd) + ((stat)->size * (cpu))))


static int _stp_stat_initialize_locks(Stat st)
{
	int i, rc;
	_stp_stat_for_each_cpu(i) {
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
}


static void _stp_stat_destroy_locks(Stat st)
{
	int i;
	_stp_stat_for_each_cpu(i) {
		stat_data *sdp = _stp_stat_per_cpu_ptr(st, i);
		(void)pthread_mutex_destroy(&sdp->lock);
	}
	(void)pthread_mutex_destroy(&st->agg->lock);
}

#endif /* _STAPDYN_STAT_RUNTIME_H_ */
