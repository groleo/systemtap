/* -*- linux-c -*- 
 * Dyninst Timer Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _LINUX_TIMER_C_
#define _LINUX_TIMER_C_

// If we're on kernels >= 2.6.17, use hrtimers.
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,17)

static unsigned long stap_hrtimer_resolution = 0;

struct stap_hrtimer_probe {
	struct hrtimer hrtimer;
	const struct stap_probe * const probe;
	int64_t intrv;
	int64_t rnd;
};

// The function signature changed in 2.6.21.
#ifdef STAPCONF_HRTIMER_REL
typedef int hrtimer_return_t;
#else
typedef enum hrtimer_restart hrtimer_return_t;
#endif


// autoconf: add get/set expires if missing (pre 2.6.28-rc1)
#ifndef STAPCONF_HRTIMER_GETSET_EXPIRES
#define hrtimer_get_expires(timer) ((timer)->expires)
#define hrtimer_set_expires(timer, time) (void)((timer)->expires = (time))
#endif

// autoconf: adapt to HRTIMER_REL -> HRTIMER_MODE_REL renaming near 2.6.21
#ifdef STAPCONF_HRTIMER_REL
#define HRTIMER_MODE_REL HRTIMER_REL
#endif


static void _stp_hrtimer_init(void)
{
	struct timespec res;
	hrtimer_get_res (CLOCK_MONOTONIC, &res);
	stap_hrtimer_resolution = timespec_to_ns(&res);
}


static inline ktime_t _stp_hrtimer_get_interval(struct stap_hrtimer_probe *stp)
{
	unsigned long nsecs;
	uint64_t i = stp->intrv;

	if (stp->rnd != 0) {
#if 1
		// XXX: why not use stp_random_pm instead of this?
	        int64_t r;
	        get_random_bytes(&r, sizeof(r));

		// ensure that r is positive
	        r &= ((uint64_t)1 << (8*sizeof(r) - 1)) - 1;
	        r = _stp_mod64(NULL, r, (2*stp->rnd+1));
	        r -= stp->rnd;
	        i += r;
#else
		i += _stp_random_pm(stp->rnd);
#endif
	}
	if (unlikely(i < stap_hrtimer_resolution))
		i = stap_hrtimer_resolution;
	nsecs = do_div(i, NSEC_PER_SEC);
	return ktime_set(i, nsecs);
}


static inline void _stp_hrtimer_update(struct stap_hrtimer_probe *stp)
{
	ktime_t time;

	time = ktime_add(hrtimer_get_expires(&stp->hrtimer),
			 _stp_hrtimer_get_interval(stp));
	hrtimer_set_expires(&stp->hrtimer, time);
}


static int
_stp_hrtimer_create(struct stap_hrtimer_probe *stp,
		    hrtimer_return_t (*function)(struct hrtimer *))
{
	hrtimer_init(&stp->hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	stp->hrtimer.function = function;
	(void)hrtimer_start(&stp->hrtimer, _stp_hrtimer_get_interval(stp),
			    HRTIMER_MODE_REL);
	return 0;
}


static void
_stp_hrtimer_cancel(struct stap_hrtimer_probe *stp)
{
	hrtimer_cancel(&stp->hrtimer);
}

#else  /* kernel version < 2.6.17 */

#error "not implemented"

#endif  /* kernel version < 2.6.17 */

#endif /* _LINUX_TIMER_C_ */
