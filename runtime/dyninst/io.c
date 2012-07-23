/* -*- linux-c -*- 
 * I/O for printing warnings, errors and debug messages
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_IO_C_
#define _STAPDYN_IO_C_

#define WARN_STRING "WARNING: "
#define ERR_STRING "ERROR: "

// XXX for now, all IO is going in-process to stdout/err

#define _stp_warn(fmt, ...) fprintf(stderr, WARN_STRING fmt "\n", ##__VA_ARGS__)
#define _stp_error(fmt, ...) fprintf(stderr, ERR_STRING fmt "\n", ##__VA_ARGS__)
#define _stp_softerror(fmt, ...) fprintf(stderr, ERR_STRING fmt "\n", ##__VA_ARGS__)
#define _stp_dbug(func, line, fmt, ...) \
	fprintf(stderr, "%s:%d: " fmt "\n", (func), (line), ##__VA_ARGS__)

#define _stp_exit() do { } while (0) // no transport, no action yet

#endif /* _STAPDYN_IO_C_ */
