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

// XXX for now, all IO is going in-process to stdout/stderr via
// _stp_out/_stp_err; see runtime/dyninst/runtime.h for initialization.

static FILE* _stp_out = NULL;
static FILE* _stp_err = NULL;


/* Clone a FILE* for private use.  On error, fallback to the original. */
static FILE* _stp_clone_file(FILE* file)
{
    int fd = dup(fileno(file));
    if (fd != -1) {
	fcntl(fd, F_SETFD, FD_CLOEXEC);
	FILE* newfile = fdopen(fd, "wb");
	if (newfile)
	    return newfile;

	close(fd);
    }
    return file;
}


/** Prints warning.
 * This function sends a warning message immediately to staprun. It
 * will also be sent over the bulk transport (relayfs) if it is
 * being used. If the last character is not a newline, then one 
 * is added. 
 * @param fmt A variable number of args.
 */
static void _stp_warn (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(_stp_err, WARN_STRING);
	vfprintf(_stp_err, fmt, args);
	fprintf(_stp_err, "\n");
	va_end(args);
}


/** Prints error message and exits.
 * This function sends an error message immediately to staprun. It
 * will also be sent over the bulk transport (relayfs) if it is
 * being used. If the last character is not a newline, then one 
 * is added. 
 *
 * After the error message is displayed, the module will be unloaded.
 * @param fmt A variable number of args.
 * @sa _stp_exit().
 */
static void _stp_error (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(_stp_err, ERR_STRING);
	vfprintf(_stp_err, fmt, args);
	fprintf(_stp_err, "\n");
	va_end(args);
// FIXME: need to exit here...
//	_stp_exit();
}


/** Prints error message.
 * This function sends an error message immediately to staprun. It
 * will also be sent over the bulk transport (relayfs) if it is
 * being used. If the last character is not a newline, then one 
 * is added. 
 *
 * @param fmt A variable number of args.
 * @sa _stp_error
 */
static void _stp_softerror (const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(_stp_err, ERR_STRING);
	vfprintf(_stp_err, fmt, args);
	fprintf(_stp_err, "\n");
	va_end(args);
}


static void _stp_dbug (const char *func, int line, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	fprintf(_stp_err, "%s:%d: ", func, line);
	vfprintf(_stp_err, fmt, args);
	fprintf(_stp_err, "\n");
	va_end(args);
}

#define _stp_exit() do { } while (0) // no transport, no action yet

#endif /* _STAPDYN_IO_C_ */
