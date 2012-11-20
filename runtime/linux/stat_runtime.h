/* -*- linux-c -*- 
 * Stat Runtime Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _LINUX_STAT_RUNTIME_H_
#define _LINUX_STAT_RUNTIME_H_

/* for the paranoid. */
#ifdef NEED_STAT_LOCKS
#define STAT_LOCK(sd)		spin_lock(&sd->lock)
#define STAT_UNLOCK(sd)		spin_unlock(&sd->lock)
#define STAT_GET_CPU()		get_cpu()
#define STAT_PUT_CPU()		put_cpu()
#else
#define STAT_LOCK(sd)		do {} while (0)
#define STAT_UNLOCK(sd)		do {} while (0)
/* get/put_cpu wrappers.  Unnecessary if caller is already atomic. */
#define STAT_GET_CPU()		smp_processor_id()
#define STAT_PUT_CPU()		do {} while (0)
#endif

#define _stp_stat_per_cpu_ptr(stat, cpu) per_cpu_ptr((stat)->sd, (cpu))

static int _stp_stat_initialize_locks(Stat st)
{
#ifdef NEED_STAT_LOCKS
	int i;
	for_each_possible_cpu(i) {
		stat_data *sdp = _stp_stat_per_cpu_ptr(st, i);
		spin_lock_init(&sdp->lock);
	}
	spin_lock_init(&st->agg->lock);
#endif
	return 0;
}

#define _stp_stat_destroy_locks(st)	do {} while (0)

#endif /* _LINUX_STAT_RUNTIME_H_ */