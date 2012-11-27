/* -*- linux-c -*- 
 * Map Runtime Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _LINUX_MAP_RUNTIME_H_
#define _LINUX_MAP_RUNTIME_H_

/* Include map spinlocks only on demand.  Otherwise, assume that
   caller does the right thing. */
#ifdef NEED_MAP_LOCKS

#define MAP_LOCK(m)	spin_lock(&(m)->lock)
#define MAP_UNLOCK(m)	spin_unlock(&(m)->lock)
#define MAP_TRYLOCK(m)	spin_trylock(&(m)->lock)

#define MAP_GET_CPU()	get_cpu()
#define MAP_PUT_CPU()	put_cpu()
#else  /* !NEED_MAP_LOCKS */

#define MAP_LOCK(m)	do {} while (0)
#define MAP_UNLOCK(m)	do {} while (0)
#define MAP_TRYLOCK(m)	1

/* get/put_cpu wrappers.  Unnecessary if caller is already atomic. */
#define MAP_GET_CPU()	smp_processor_id()
#define MAP_PUT_CPU()	do {} while (0)

#endif


static int _stp_map_initialize_lock(MAP m)
{
#ifdef NEED_MAP_LOCKS
	spin_lock_init(&m->lock);
#endif
	return 0;
}

#define _stp_map_destroy_lock(m)	do {} while (0)

struct pmap {
	MAP agg;	/* aggregation map */
	MAP map[];	/* per-cpu maps */
};

static inline PMAP _stp_pmap_alloc(void)
{
	/* Called from module_init, so user context, may sleep alloc. */
	return _stp_kzalloc_gfp((1 + NR_CPUS) * sizeof(MAP),
				STP_ALLOC_SLEEP_FLAGS);
}

static inline MAP _stp_pmap_get_agg(PMAP p)
{
	return p->agg;
}

static inline void _stp_pmap_set_agg(PMAP p, MAP agg)
{
	p->agg = agg;
}

static inline MAP _stp_pmap_get_map(PMAP p, unsigned cpu)
{
	if (cpu >= NR_CPUS)
		cpu = 0;
	return p->map[cpu];
}

static inline void _stp_pmap_set_map(PMAP p, MAP m, unsigned cpu)
{
	if (cpu >= NR_CPUS)
		cpu = 0;
	p->map[cpu] = m;
}

#endif /* _LINUX_MAP_RUNTIME_H_ */
