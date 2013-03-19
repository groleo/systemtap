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

#include "offptr.h"

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


/** Stat struct. Maps do not need this */
typedef struct _Stat {
	struct _Hist hist;

	/* aggregated data */
	offptr_t oagg;

	/* The stat data is a "per-cpu" array.  */
        offptr_t osd[];
} *Stat;

static Stat _stp_stat_alloc(size_t stat_data_size)
{
	int i;
	void *mem;
	Stat st;

	size_t stat_size = sizeof(struct _Stat)
		+ sizeof(offptr_t) * _stp_runtime_num_contexts;

	size_t total_size = stat_size +
		stat_data_size * (_stp_runtime_num_contexts + 1);

	if (stat_data_size < sizeof(stat_data))
		return NULL;

	/* NB: This is done as one big allocation, then assigning offptrs to
	 * each sub-piece.  (See _stp_pmap_new for more explanation) */
	st = mem = _stp_shm_zalloc(total_size);
	if (st == NULL)
		return NULL;

	mem += stat_size;
	offptr_set(&st->oagg, mem);

	for_each_possible_cpu(i) {
		mem += stat_data_size;
		offptr_set(&st->osd[i], mem);
	}

	return st;
}

static void _stp_stat_free(Stat st)
{
	_stp_shm_free(st);
}

static inline stat_data* _stp_stat_get_agg(Stat st)
{
	return offptr_get(&st->oagg);
}

static inline stat_data* _stp_stat_per_cpu_ptr(Stat st, int cpu)
{
	return offptr_get(&st->osd[cpu]);
}


static int _stp_stat_initialize_locks(Stat st)
{
#ifdef NEED_STAT_LOCKS
	int i, rc;
	stat_data *sdp;
	for_each_possible_cpu(i) {
		sdp = _stp_stat_per_cpu_ptr (st, i);

		if ((rc = stp_pthread_mutex_init_shared(&sdp->lock))
		    != 0) {
			return rc;
		}
	}
	sdp = _stp_stat_get_agg (st);
	if ((rc = stp_pthread_mutex_init_shared(&sdp->lock)) != 0) {
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
	stat_data *sdp;
	for_each_possible_cpu(i) {
		sdp = _stp_stat_per_cpu_ptr(st, i);
		(void)pthread_mutex_destroy(&sdp->lock);
	}
	sdp = _stp_stat_get_agg (st);
	(void)pthread_mutex_destroy(&sdp->lock);
#endif
}

#endif /* _STAPDYN_STAT_RUNTIME_H_ */
