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

#ifdef STP_BULKMODE
#error "Bulk mode output (percpu files) not supported for --runtime=dyninst"
#endif
#ifdef STP_USE_RING_BUFFER
#error "Ring buffer output not supported for --runtime=dyninst"
#endif
#if defined(RELAY_GUEST) || defined(RELAY_HOST)
#error "Relay host/guest output not supported for --runtime=dyninst"
#endif

#include "vsprintf.c"

typedef struct {
	size_t buf_alloc;
	size_t buf_used;
	void *buf;
} _stp_pbuf_t;

static _stp_pbuf_t *_stp_pbuf = NULL;

static void _stp_print_kernel_info(char *vstr, int ctx, int num_probes)
{
	// nah...
}

static int _stp_print_init(void)
{
	int i;

	/* Allocate an array: _stp_pbuf_t[_stp_runtime_num_contexts] */
	_stp_pbuf = calloc(sizeof(_stp_pbuf_t) * _stp_runtime_num_contexts, 1);
	if (_stp_pbuf == NULL)
		return -ENOMEM;

	/* Let's go ahead and pre-allocate the buffers. Note they
	   might grow later.  */
	for (i = 0; i < _stp_runtime_num_contexts; i++) {
		_stp_pbuf[i].buf_alloc = STP_BUFFER_SIZE;
		_stp_pbuf[i].buf = malloc(STP_BUFFER_SIZE);
		if (_stp_pbuf[i].buf == NULL) {
			_stp_print_cleanup();
			return -ENOMEM;
		}
	}
	return 0;
}

static void _stp_print_cleanup(void)
{
	int i;

	if (_stp_pbuf == NULL)
		return;

	for (i = 0; i < _stp_runtime_num_contexts; i++) {
		if (_stp_pbuf[i].buf)
			free(_stp_pbuf[i].buf);
	}
	if (_stp_pbuf) {
		free(_stp_pbuf);
		_stp_pbuf = NULL;
	}
}

static inline void _stp_print_flush(void)
{
	_stp_pbuf_t *pbuf;

	fflush(_stp_err);

	pbuf = &_stp_pbuf[_stp_runtime_get_data_index()];
	if (pbuf->buf_used) {
		fwrite(pbuf->buf, pbuf->buf_used, 1, _stp_out);
		fflush(_stp_out);
		pbuf->buf_used = 0;
	}
}

static void * _stp_reserve_bytes (int numbytes)
{
	_stp_pbuf_t *pbuf;
	size_t size;

	pbuf = &_stp_pbuf[_stp_runtime_get_data_index()];
	size = pbuf->buf_used + numbytes;
	if (size > pbuf->buf_alloc) {
		/* XXX: Should the new size be a multiple of
		   STP_BUFFER_SIZE? */
		void *buf = realloc(pbuf->buf, size);
		if (!buf)
			return NULL;
		pbuf->buf = buf;
		pbuf->buf_alloc = size;
	}
	void *ret = pbuf->buf + pbuf->buf_used;
	pbuf->buf_used += numbytes;
	return ret;
}

static void _stp_unreserve_bytes (int numbytes)
{
	_stp_pbuf_t *pbuf;

	pbuf = &_stp_pbuf[_stp_runtime_get_data_index()];
	if (unlikely(numbytes <= 0 || numbytes > pbuf->buf_used))
		return;

	pbuf->buf_used -= numbytes;
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

static void _stp_print_char (const char c)
{
	char *p = _stp_reserve_bytes(1);;

	if (p) {
		*p = c;
	}
}

#endif /* _STAPDYN_PRINT_C_ */
