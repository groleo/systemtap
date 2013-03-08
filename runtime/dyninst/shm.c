/* -*- linux-c -*-
 * Shared Memory Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_SHM_H_
#define _STAPDYN_SHM_H_

#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static char _stp_shm_name[NAME_MAX] = { '\0' };
static int _stp_shm_fd = -1;
static int _stp_shm_page_size = -1;
static off_t _stp_shm_size = 0;
static off_t _stp_shm_allocated = 0;
static void *_stp_shm_base = NULL;

static const char *_stp_shm_init(void);
static int _stp_shm_connect(const char *name);
static void *_stp_shm_alloc(size_t size);
static void *_stp_shm_zalloc(size_t size);
static void _stp_shm_free(void *ptr);
static void _stp_shm_finalize(void);
static void _stp_shm_destroy(void);


#ifdef DEBUG_SHM
#define shm_dbug(fmt, args...) _stp_dbug(__FUNCTION__, __LINE__, fmt, ##args)
#else
#define shm_dbug(fmt, args...)
#endif


// Create and initialize the shared memory for this module.
static const char *_stp_shm_init(void)
{
	char *name, name_buf[] = "/dev/shm/stapdyn.XXXXXX";
	long page_size;
	int fd;
	void *base;

	// If we already have shared memory, carry on...
	if (_stp_shm_base)
		return _stp_shm_name;

	// Find the increment to use in growing our memory size.  Since this is
	// used in mmap, only multiples of the page size make sense.
	page_size = sysconf(_SC_PAGESIZE);
	if (page_size < 1)
		return NULL;

	// Create a unique name for our shared memory, assuming that it will
	// exist in /dev/shm as Linux normally does.  The actual name to use is
	// then just the last '/' and on.
	if (!mktemp(name_buf) || !(name = strrchr(name_buf, '/')))
		return NULL;

	// Create the actual share memory.  The O_EXCL saves us from the
	// possible mktemp race.  Only USR file mode bits are added, because
	// that's all that ptrace will allow to attach anyway.  (Unless you're
	// root, but that deserves much larger consideration.)
	fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
	if (fd < 0)
		return NULL;

	// Start the shared memory sized with just one unit.
	if (ftruncate(fd, page_size) < 0)
		goto err_fd;

	// Now finally map it into this process.
	base = mmap(NULL, page_size, PROT_READ | PROT_WRITE,
		    MAP_SHARED, fd, 0);
	if (base == MAP_FAILED)
		goto err_fd;

	// We're done; set globals and go home!
	strlcpy(_stp_shm_name, name, sizeof(_stp_shm_name));
	_stp_shm_fd = fd;
	_stp_shm_page_size = page_size;
	_stp_shm_size = page_size;
	_stp_shm_allocated = 0;
	_stp_shm_base = base;
	shm_dbug("initialized %s @ %p", _stp_shm_name, _stp_shm_base);
	return _stp_shm_name;

err_fd:
	close(fd);
	shm_unlink(_stp_shm_name);
	return NULL;
}


static int _stp_shm_connect(const char *name)
{
	int rc = 0, fd = -1;
	struct stat st;
	void *base;

	// NB: since we're just connecting, we won't set _stp_shm_name,
	// so _stp_shm_destroy won't try to unlink it from this process.

	// Open the pre-existing shared memory
	fd = shm_open(name, O_RDWR, 0);
	if (fd < 0)
		rc = fd;

	// Find out the shared memory's size.
	// (This implies that the main process is done allocating.)
	if (rc == 0)
		rc = fstat(fd, &st);

	// Map it into this process.
	if (rc == 0) {
		base = mmap(NULL, st.st_size, PROT_READ | PROT_WRITE,
			    MAP_SHARED, fd, 0);
		if (base == MAP_FAILED)
			rc = -1;
		else
			_stp_shm_base = base;
	}

	if (fd >= 0)
		close(fd);

	if (rc == 0)
		shm_dbug("connected pid %d to %s @ %p", getpid(), name, _stp_shm_base);
	return rc;
}


// Allocate space from shared memory
static void *_stp_shm_alloc(size_t size)
{
	void *alloc;

	// We're not initialized or already finalized.
	// Either way, we're not taking new requests.
	if (_stp_shm_fd < 0)
		return NULL;

	// Round up to 8-byte aligned sizes, just to be a little safer
	size = (size + 7) & ~7;
	if (size <= 0)
		return NULL; // either 0 requested or overflow

	// Check if more memory is needed.
	if (_stp_shm_size - _stp_shm_allocated < size) {
		void *new_base;

		// Round up to the nearest page size
		off_t new_size = _stp_shm_allocated + size;
		new_size += _stp_shm_page_size - 1;
		new_size /= _stp_shm_page_size;
		new_size *= _stp_shm_page_size;
		if (new_size < _stp_shm_allocated ||
		    new_size - _stp_shm_allocated < size)
			return NULL; // math overflow?

		// Try to resize the underlying file.
		if (ftruncate(_stp_shm_fd, new_size) < 0)
			return NULL;

		// Try to remap the address in memory.
		new_base = mremap(_stp_shm_base, _stp_shm_size,
				  new_size, MREMAP_MAYMOVE);
		if (new_base == MAP_FAILED)
			return NULL;

		// Update globals
		_stp_shm_size = new_size;
		_stp_shm_base = new_base;
	}

	// Finally return some memory.
	alloc = _stp_shm_base + _stp_shm_allocated;
	_stp_shm_allocated += size;
	return alloc;
}


// Allocate zeroed space from shared memory
static void *_stp_shm_zalloc(size_t size)
{
	void *ptr = _stp_shm_alloc(size);
	if (ptr)
		memset(ptr, 0, size);
	return ptr;
}


static void _stp_shm_free(void *ptr __attribute__((unused)))
{
	// NO-OP; we don't try to reclaim individual pieces.
}


// Signal that we're done allocating in shared memory.
// It won't be allowed to grow or move from here on out.
static void _stp_shm_finalize(void)
{
	if (_stp_shm_fd >= 0) {
		close(_stp_shm_fd);
		_stp_shm_fd = -1;
	}
	shm_dbug("mapped %zi bytes @ %p, used %zi", _stp_shm_size,
		 _stp_shm_base, _stp_shm_allocated);
}


// Tear down everything we created... *sniff*
// NB: Make sure not to reference any memory within after this!
// (Other processes may still have their own mmap reference though.)
static void _stp_shm_destroy(void)
{
	if (_stp_shm_base) {
		munmap(_stp_shm_base, _stp_shm_size);
		_stp_shm_base = NULL;
		_stp_shm_size = 0;
		_stp_shm_allocated = 0;
	}

	if (_stp_shm_fd >= 0) {
		close(_stp_shm_fd);
		_stp_shm_fd = -1;
	}

	if (_stp_shm_name[0]) {
		shm_unlink(_stp_shm_name);
		_stp_shm_name[0] = '\0';
	}
}


#endif /* _STAPDYN_SHM_H_ */
