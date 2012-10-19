/* -*- linux-c -*-
 * Statistics Aggregation
 * Copyright (C) 2005-2008, 2012 Red Hat Inc.
 * Copyright (C) 2006 Intel Corporation
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */
#ifndef _STAT_C_
#define _STAT_C_

/** @file stat.c
 * @brief Statistics Aggregation
 */
/** @addtogroup stat Statistics Aggregation
 * The Statistics aggregations keep per-cpu statistics. You
 * must create all aggregations at probe initialization and it is
 * best to not read them until probe exit. If you must read them
 * while probes are running, the values may be slightly off due
 * to a probe updating the statistics of one cpu while another cpu attempts
 * to read the same data. This will also negatively impact performance.
 *
 * If you have a need to poll Stat data while probes are running, and
 * you want to be sure the data is accurate, you can do
 * @verbatim
#define NEED_STAT_LOCKS 1
@endverbatim
 * This will insert per-cpu spinlocks around all accesses to Stat data, 
 * which will reduce performance some.
 *
 * Stats keep track of count, sum, min and max. Average is computed
 * from the sum and count when required. Histograms are optional.
 * If you want a histogram, you must set "type" to HIST_LOG
 * or HIST_LINEAR when you call _stp_stat_init().
 *
 * @{
 */

#ifndef __KERNEL__
#include <pthread.h>
#endif

#include "stat-common.c"

/* for the paranoid. */
#if NEED_STAT_LOCKS == 1
#ifdef __KERNEL__
#define STAT_LOCK(sd) spin_lock(&sd->lock)
#define STAT_UNLOCK(sd) spin_unlock(&sd->lock)
#else
#define STAT_LOCK(sd) pthread_mutex_lock(&sd->lock)
#define STAT_UNLOCK(sd) pthread_mutex_unlock(&sd->lock)
#endif
#else
#define STAT_LOCK(sd) ;
#define STAT_UNLOCK(sd) ;
#endif

/** Stat struct for stat.c. Maps do not need this */
struct _Stat {
	struct _Hist hist;

	/*
	 * In kernel-mode, the stat data is per-cpu data (allocated
	 * with _stp_alloc_percpu()) stored in 'sd'. In dyninst-mode,
	 * the stat data is thread local storage.
	 */
#ifdef __KERNEL__
	stat_data *sd;
#else
	struct tls_data_container_t container;
#endif
	/* aggregated data */   
	stat_data *agg;  
};

typedef struct _Stat *Stat;

#ifndef __KERNEL__
#if NEED_STAT_LOCKS == 1
static int _stp_stat_tls_object_init(struct tls_data_object_t *obj)
{
	stat_data *sd = container_of(obj, stat_data, object);

	int rc;
	if ((rc = pthread_mutex_init(&sd->lock, NULL)) != 0) {
		_stp_error("Couldn't initialize stat mutex: %d\n", rc);
		return -1;
	}
	return 0;
}

static void _stp_stat_tls_object_free(struct tls_data_object_t *obj)
{
	stat_data *sd = container_of(obj, stat_data, object);
	(void)pthread_mutex_destroy(&sd->lock);
}
#endif	/* NEED_STAT_LOCKS == 1 */
#endif	/* !__KERNEL__ */

/** Initialize a Stat.
 * Call this during probe initialization to create a Stat.
 *
 * @param type HIST_NONE, HIST_LOG, or HIST_LINEAR
 *
 * For HIST_LOG, the following additional parametrs are required:
 * @param buckets - An integer specifying the number of buckets.
 *
 * For HIST_LINEAR, the following additional parametrs are required:
 * @param start - An integer. The start of the histogram.
 * @param stop - An integer. The stopping value. Should be > start.
 * @param interval - An integer. The interval. 
 */
static Stat _stp_stat_init (int type, ...)
{
	int size, buckets=0, start=0, stop=0, interval=0;
#ifdef __KERNEL__
	stat_data *sd, *agg;
#else
	stat_data *agg;
#endif
	Stat st;

	if (type != HIST_NONE) {
		va_list ap;
		va_start (ap, type);
		
		if (type == HIST_LOG) {
			buckets = HIST_LOG_BUCKETS;
		} else {
			start = va_arg(ap, int);
			stop = va_arg(ap, int);
			interval = va_arg(ap, int);

			buckets = _stp_stat_calc_buckets(stop, start, interval);
			if (!buckets)
				return NULL;
		}
		va_end (ap);
	}
	/* Called from module_init, so user context, may sleep alloc. */
	st = (Stat) _stp_kmalloc_gfp (sizeof(struct _Stat), STP_ALLOC_SLEEP_FLAGS);
	if (st == NULL)
		return NULL;
	
	size = buckets * sizeof(int64_t) + sizeof(stat_data);	
#ifdef __KERNEL__
	sd = (stat_data *) _stp_alloc_percpu (size);
	if (sd == NULL)
		goto exit1;
	st->sd = sd;

#if NEED_STAT_LOCKS == 1
	{
		int i;
		for_each_possible_cpu(i) {
			stat_data *sdp = per_cpu_ptr (sd, i);
			spin_lock_init(sdp->lock);
		}
	}
#endif	/* NEED_STAT_LOCKS == 1 */

#else  /* !__KERNEL__ */

#if NEED_STAT_LOCKS == 1
	if (_stp_tls_data_container_init(&st->container, size,
					 &_stp_stat_tls_object_init,
					 &_stp_stat_tls_object_free) != 0)
#else  /* NEED_STAT_LOCKS !=1 */
	if (_stp_tls_data_container_init(&st->container, size,
					 NULL, NULL) != 0)
#endif	/* NEED_STAT_LOCKS != 1 */
		goto exit1;
#endif	/* !__KERNEL__ */
	
	agg = (stat_data *)_stp_kmalloc_gfp(size, STP_ALLOC_SLEEP_FLAGS);
	if (agg == NULL)
		goto exit2;

	st->hist.type = type;
	st->hist.start = start;
	st->hist.stop = stop;
	st->hist.interval = interval;
	st->hist.buckets = buckets;
	st->agg = agg;
	return st;

exit2:
#ifdef __KERNEL__
	_stp_free_percpu (sd);
#else
	_stp_tls_data_container_cleanup(&st->container);
#endif
exit1:
	_stp_kfree (st);
	return NULL;
}

/** Delete Stat.
 * Call this to free all memory allocated during initialization.
 *
 * @param st Stat
 */
static void _stp_stat_del (Stat st)
{
	if (st) {
#ifdef __KERNEL__
		_stp_free_percpu (st->sd);
#else  /* !__KERNEL__ */
		_stp_tls_data_container_cleanup(&st->container);
#endif /* !__KERNEL__ */
		_stp_kfree (st->agg);
		_stp_kfree (st);
	}
}
	
/** Add to a Stat.
 * Add an int64 to a Stat.
 *
 * @param st Stat
 * @param val Value to add
 */
static void _stp_stat_add (Stat st, int64_t val)
{
#ifdef __KERNEL__
	stat_data *sd = per_cpu_ptr (st->sd, get_cpu());
#else
	struct tls_data_object_t *obj;
	stat_data *sd;

	obj = _stp_tls_get_per_thread_ptr(&st->container);
	if (!obj)
		return;
	sd = container_of(obj, stat_data, object);
#endif
	STAT_LOCK(sd);
	__stp_stat_add (&st->hist, sd, val);
	STAT_UNLOCK(sd);
	put_cpu();
}


static void _stp_stat_clear_data (Stat st, stat_data *sd)
{
        int j;
        sd->count = sd->sum = sd->min = sd->max = 0;
        if (st->hist.type != HIST_NONE) {
                for (j = 0; j < st->hist.buckets; j++)
                        sd->histogram[j] = 0;
        }
}

/** Get Stats.
 * Gets the aggregated Stats for all CPUs.
 *
 * If NEED_STAT_LOCKS is set, you MUST call STAT_UNLOCK()
 * when you are finished with the returned pointer.
 *
 * @param st Stat
 * @param clear Set if you want the data cleared after the read. Useful
 * for polling.
 * @returns A pointer to a stat.
 */
static stat_data *_stp_stat_get (Stat st, int clear)
{
	int i, j;
	stat_data *agg = st->agg;
	stat_data *sd;
#ifndef __KERNEL__
	struct tls_data_object_t *obj;
#endif
	STAT_LOCK(agg);
	_stp_stat_clear_data (st, agg);

#ifdef __KERNEL__
	for_each_possible_cpu(i) {
		sd = per_cpu_ptr (st->sd, i);
#else
	TLS_DATA_CONTAINER_LOCK(&st->container);
	for_each_tls_data(obj, &st->container) {
		sd = container_of(obj, stat_data, object);
#endif
		STAT_LOCK(sd);
		if (sd->count) {
			if (agg->count == 0) {
				agg->min = sd->min;
				agg->max = sd->max;
			}
			agg->count += sd->count;
			agg->sum += sd->sum;
			if (sd->max > agg->max)
				agg->max = sd->max;
			if (sd->min < agg->min)
				agg->min = sd->min;
			if (st->hist.type != HIST_NONE) {
				for (j = 0; j < st->hist.buckets; j++)
					agg->histogram[j] += sd->histogram[j];
			}
			if (clear)
				_stp_stat_clear_data (st, sd);
		}
		STAT_UNLOCK(sd);
	}
#ifndef __KERNEL__
	TLS_DATA_CONTAINER_UNLOCK(&st->container);
#endif
	return agg;
}


/** Clear Stats.
 * Clears the Stats.
 *
 * @param st Stat
 */
static void _stp_stat_clear (Stat st)
{
#ifdef __KERNEL__
	int i;
	for_each_possible_cpu(i) {
		stat_data *sd = per_cpu_ptr (st->sd, i);
#else
	struct tls_data_object_t *obj;
	TLS_DATA_CONTAINER_LOCK(&st->container);
	for_each_tls_data(obj, &st->container) {
		stat_data *sd = container_of(obj, stat_data, object);
#endif
		STAT_LOCK(sd);
		_stp_stat_clear_data (st, sd);
		STAT_UNLOCK(sd);
	}
#ifndef __KERNEL__
	TLS_DATA_CONTAINER_UNLOCK(&st->container);
#endif
}
/** @} */
#endif /* _STAT_C_ */

