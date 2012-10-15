/* -*- linux-c -*- 
 * Dyninst Timer Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_TIMER_C_
#define _STAPDYN_TIMER_C_

#include <signal.h>
#include <time.h>

#ifndef NSEC_PER_SEC
#define NSEC_PER_SEC 1000000000L
#endif

struct stap_hrtimer_probe {
	struct sigevent sigev;
	timer_t timer_id;
	struct itimerspec its;
	struct stap_probe * const probe;
	int64_t intrv;
	int64_t rnd;
};


static void _stp_hrtimer_init(void)
{
	return;
}


static int
_stp_hrtimer_create(struct stap_hrtimer_probe *shp, void (*function)(sigval_t))
{
	int rc;

	/* Create the timer. */
	shp->sigev.sigev_notify = SIGEV_THREAD;
	shp->sigev.sigev_value.sival_ptr = shp;
	shp->sigev.sigev_notify_function = function;
	shp->sigev.sigev_notify_attributes = NULL;
	rc = timer_create(CLOCK_MONOTONIC, &shp->sigev, &shp->timer_id);
	if (rc) {
		return rc;
	}

	/* Specify a timer with the correct initial value (possibly
	 * randomized a bit).
	 *
	 * If this isn't a randomized timer probe, go ahead and set
	 * up the repeating interval values.
	 *
	 * The probe's interval is in nanoseconds,
	 * but in a int64_t. So, break it down into seconds and
	 * (leftover) nanoseconds so it will fit in a 'struct
	 * timespec'.
	 */

	if (shp->rnd == 0) {
		shp->its.it_value.tv_sec = (shp->its.it_interval.tv_sec	\
					    = shp->intrv / NSEC_PER_SEC);
		shp->its.it_value.tv_nsec = (shp->its.it_interval.tv_nsec \
					     = shp->intrv % NSEC_PER_SEC);
	}
	else {
		int64_t i = shp->intrv + _stp_random_u(shp->rnd);
		shp->its.it_value.tv_sec = i / NSEC_PER_SEC;
		shp->its.it_value.tv_nsec = i % NSEC_PER_SEC;
	}
	rc = timer_settime(shp->timer_id, 0, &shp->its, NULL);
	return rc;
}


static void _stp_hrtimer_update(struct stap_hrtimer_probe *shp)
{
	int64_t i = shp->intrv;

	/* The timer only needs updating if this is a randomized timer
	 * probe */
	if (shp->rnd == 0)
		return;

	/* The probe's interval is in nanoseconds, but in a
	 * int64_t. So, break it down into seconds and (leftover)
	 * nanoseconds.
	 */
	i += _stp_random_u(shp->rnd);
	shp->its.it_value.tv_sec = i / NSEC_PER_SEC;
	shp->its.it_value.tv_nsec = i % NSEC_PER_SEC;
	timer_settime(shp->timer_id, 0, &shp->its, NULL);
}


static void _stp_hrtimer_cancel(struct stap_hrtimer_probe *shp)
{
	(void) timer_delete(shp->timer_id);
}

#endif /* _STAPDYN_TIMER_C_ */
