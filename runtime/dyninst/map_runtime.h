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

#define _stp_map_per_cpu_ptr(m, cpu)	&((m)[(cpu)])

#endif /* _STAPDYN_MAP_RUNTIME_H_ */
