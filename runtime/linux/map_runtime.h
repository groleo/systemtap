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

#define _stp_map_for_each_cpu(cpu)	for_each_possible_cpu((cpu))
#define _stp_map_per_cpu_ptr(m, cpu)	per_cpu_ptr((m), (cpu))

#endif /* _LINUX_MAP_RUNTIME_H_ */
