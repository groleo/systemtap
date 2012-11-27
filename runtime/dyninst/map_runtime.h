/* -*- linux-c -*- 
 * Map Runtime Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_MAP_RUNTIME_H_
#define _STAPDYN_MAP_RUNTIME_H_

#include <pthread.h>

#ifdef NEED_MAP_LOCKS
#define MAP_LOCK(m)	pthread_mutex_lock(&(m)->lock)
#define MAP_UNLOCK(m)	pthread_mutex_unlock(&(m)->lock)
#else
#define MAP_LOCK(sd)	do {} while (0)
#define MAP_UNLOCK(sd)	do {} while (0)
#endif

/* Note that pthread_mutex_trylock()'s return value is opposite of the
 * kernel's spin_trylock(), so we invert the return value of
 * pthread_mutex_trylock(). */
#define MAP_TRYLOCK(m)	(!pthread_mutex_trylock(&(m)->lock))

#define MAP_GET_CPU()	STAT_GET_CPU()
#define MAP_PUT_CPU()	STAT_PUT_CPU()

static int _stp_map_initialize_lock(MAP m)
{
#ifdef NEED_MAP_LOCKS
	int rc;

	if ((rc = pthread_mutex_init(&m->lock, NULL)) != 0) {
		_stp_error("Couldn't initialize map mutex: %d\n", rc);
		return rc;
	}
#endif
	return 0;
}

static void _stp_map_destroy_lock(MAP m)
{
#ifdef NEED_MAP_LOCKS
	(void)pthread_mutex_destroy(&m->lock);
#endif
}

struct pmap {
	offptr_t oagg;    /* aggregation map */
	offptr_t omap[];  /* per-cpu maps */
};

static inline PMAP _stp_pmap_alloc(void)
{
	return calloc((1 + _stp_runtime_num_contexts), sizeof(offptr_t));
}

static inline MAP _stp_pmap_get_agg(PMAP p)
{
	return offptr_get(&p->oagg);
}

static inline void _stp_pmap_set_agg(PMAP p, MAP agg)
{
	offptr_set(&p->oagg, agg);
}

static inline MAP _stp_pmap_get_map(PMAP p, unsigned cpu)
{
	if (cpu >= _stp_runtime_num_contexts)
		cpu = 0;
	return offptr_get(&p->omap[cpu]);
}

static inline void _stp_pmap_set_map(PMAP p, MAP m, unsigned cpu)
{
	if (cpu >= _stp_runtime_num_contexts)
		cpu = 0;
	offptr_set(&p->omap[cpu], m);
}

#endif /* _STAPDYN_MAP_RUNTIME_H_ */
