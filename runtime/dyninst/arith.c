/* -*- linux-c -*- */
/* Math functions
 * Copyright (C) 2005-2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_ARITH_C_ 
#define _STAPDYN_ARITH_C_

/** @file arith.
 * @brief Implements various arithmetic-related helper functions
 */


/** Divide x by y.  In case of division-by-zero,
 * set context error string, and return 0
 */
static int64_t _stp_div64 (const char **error, int64_t x, int64_t y)
{
	// check for division-by-zero
	if (unlikely (y == 0)) {
		if (error) *error = "division by 0";
		return 0;
	}

	if (unlikely (y == -1))
		return -x;

	return x/y;
}


/** Modulo x by y.  In case of division-by-zero,
 * set context error string, and return any 0
 */
static int64_t _stp_mod64 (const char **error, int64_t x, int64_t y)
{
	// check for division-by-zero
	if (unlikely (y == 0)) {
		if (error) *error = "division by 0";
		return 0;
	}

	if (unlikely (y == 1 || y == -1))
		return 0;

	return x%y;
}


static unsigned long _stp_random_u_init(void)
{
	unsigned long seed;
	ssize_t count = 0;

	int fd = open("/dev/urandom", O_RDONLY);
	if (fd >= 0) {
		count = read(fd, &seed, sizeof(seed));
		close(fd);
	}

	/* If urandom fails for any reason, this is a crude fallback */
	if (count != sizeof(seed)) {
		struct timespec ts;
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
		seed = ts.tv_sec * NSEC_PER_SEC + ts.tv_nsec;
	}

	return seed;
}

/** Return a random integer between 0 and n - 1.
 * @param n how far from zero to go.  Make it positive but less than a million or so.
 */
static unsigned long _stp_random_u (unsigned long n)
{
	static unsigned long seed;
	static int initialized_p = 0;

	if (unlikely (! initialized_p)) {
		seed = _stp_random_u_init();
		initialized_p = 1;
	}

	/* from glibc rand man page */
	seed = seed * 1103515245 + 12345;

	return (n == 0 ? 0 : seed % n);
}


/** Return a random integer between -n and n.
 * @param n how far from zero to go.  Make it positive but less than a million or so.
 */
static int _stp_random_pm (unsigned n)
{
        return -(int)n + (int)_stp_random_u (2*n + 1);
}

#endif /* _STAPDYN_ARITH_C_ */

