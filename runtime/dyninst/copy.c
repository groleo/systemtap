/* -*- linux-c -*- 
 * Copy from user space functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_COPY_C_
#define _STAPDYN_COPY_C_

#include "stp_string.c"


static int _stp_mem_fd = -1;

static inline __must_check long __copy_from_user(void *to,
		const void __user * from, unsigned long n)
{
	int rc = 0;

	/*
	 * The pread syscall is faster than lseek()/read() (since it
	 * is only one syscall). Also, if we used lseek()/read() we
	 * couldn't use a cached fd - since 2 threads might hit this
	 * code at the same time and the 2nd lseek() might finish
	 * before the 1st read()...
	 */
	if (pread(_stp_mem_fd, to, n, (off_t)from) != n)
		rc = -EFAULT;
	return rc;
}

static inline __must_check long __copy_to_user(void *to, const void *from,
					       unsigned long n)
{
	int rc = 0;

	/*
	 * The pwrite syscall is faster than lseek()/write() (since it
	 * is only one syscall).
	 */
	if (pwrite(_stp_mem_fd, to, n, (off_t)from) != n)
		rc = -EFAULT;
	return rc;
}

static long
_stp_strncpy_from_user(char *dst, const char *src, long count)
{
	long i;
	if (count <= 0)
		return -EINVAL;
	for (i = 0; i < count; ++i) {
		if (__copy_from_user(dst, src++, 1))
			return -EFAULT;
		if (*dst++ == 0)
			return i;
	}
	*(dst - 1) = 0;
	return i - 1;
}

static unsigned long _stp_copy_from_user(char *dst, const char *src, unsigned long count)
{
	if (count && __copy_from_user(dst, src, count) == 0)
		return 0;
	return count;
}

#endif /* _STAPDYN_COPY_C_ */

