/* dyninst probe locking header file
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_PROBE_LOCK_H
#define _STAPDYN_PROBE_LOCK_H

#include <pthread.h>

typedef pthread_rwlock_t rwlock_t;

struct stp_probe_lock {
	#ifdef STP_TIMING
	atomic_t *skipped;
	#endif
	rwlock_t *lock;
	unsigned write_p;
};


#define rwlock_init(x) \
	pthread_rwlock_init(x, NULL)


static void
stp_unlock_probe(const struct stp_probe_lock *locks, unsigned num_locks)
{
	unsigned i;
	for (i = num_locks; i-- > 0;) {
		pthread_rwlock_unlock(locks[i].lock);
	}
}


static unsigned
stp_lock_probe(const struct stp_probe_lock *locks, unsigned num_locks)
{
#if 0 // XXX: should we bother with trylocks in pure userspace?
	unsigned i, retries = 0;
	for (i = 0; i < num_locks; ++i) {
		if (locks[i].write_p)
			while (!pthread_rwlock_trywrlock(locks[i].lock)) {
				if (++retries > MAXTRYLOCK)
					goto skip;
				udelay (TRYLOCKDELAY);
			}
		else
			while (!pthread_rwlock_tryrdlock(locks[i].lock)) {
				if (++retries > MAXTRYLOCK)
					goto skip;
				udelay (TRYLOCKDELAY);
			}
	}
	return 1;

skip:
	atomic_inc(skipped_count());
#ifdef STP_TIMING
	atomic_inc(locks[i].skipped);
#endif
	stp_unlock_probe(locks, i);
	return 0;
#else
	unsigned i;
	for (i = 0; i < num_locks; ++i) {
		if (locks[i].write_p)
			pthread_rwlock_wrlock(locks[i].lock);
		else
			pthread_rwlock_rdlock(locks[i].lock);
	}
	return 1;
#endif
}


#endif /* _STAPDYN_PROBE_LOCK_H */

