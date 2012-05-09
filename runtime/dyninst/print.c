/* -*- linux-c -*- 
 * Print Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_PRINT_C_
#define _STAPDYN_PRINT_C_

#include "vsprintf.c"

static size_t _stp_print_buf_alloc = 0;
static size_t _stp_print_buf_used = 0;
static void * _stp_print_buf;

static void _stp_print_kernel_info(char *vstr, int ctx, int num_probes)
{
	// nah...
}

static inline void _stp_print_flush(void)
{
	fflush(stderr);
	if (_stp_print_buf_used) {
		fwrite(_stp_print_buf, _stp_print_buf_used, 1, stdout);
		fflush(stdout);
		_stp_print_buf_used = 0;
	}
}

static void * _stp_reserve_bytes (int numbytes)
{
	size_t size = _stp_print_buf_used + numbytes;
	if (size > _stp_print_buf_alloc) {
		void *buf = realloc(_stp_print_buf, size);
		if (!buf)
			return NULL;
		_stp_print_buf = buf;
		_stp_print_buf_alloc = size;
	}
	void *ret = _stp_print_buf + _stp_print_buf_used;
	_stp_print_buf_used += numbytes;
	return ret;
}

static void _stp_unreserve_bytes (int numbytes)
{
	if (unlikely(numbytes <= 0 || numbytes > _stp_print_buf_used))
		return;

	_stp_print_buf_used -= numbytes;
}

static void _stp_printf (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	_stp_vsnprintf(NULL, 0, fmt, args);
	va_end(args);
}

static void _stp_print (const char *str)
{
    _stp_printf("%s", str);
}

#endif /* _STAPDYN_PRINT_C_ */

