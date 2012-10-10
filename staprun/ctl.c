/* -*- linux-c -*-
 *
 * ctl.c - staprun control channel
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 *
 * Copyright (C) 2012 Red Hat Inc.
 */

#include "staprun.h"

#define CTL_CHANNEL_NAME ".cmd"

int init_ctl_channel(const char *name, int verb)
{
	char buf[PATH_MAX];
	struct statfs st;
	int old_transport = 0;

        if (0) goto out; /* just to defeat gcc warnings */

#ifdef HAVE_OPENAT
        if (relay_basedir_fd >= 0) {
                strncpy(buf, CTL_CHANNEL_NAME, PATH_MAX);
                control_channel = openat(relay_basedir_fd, CTL_CHANNEL_NAME, O_RDWR);
                dbug(2, "Opened %s (%d)\n", CTL_CHANNEL_NAME, control_channel);

                /* NB: Extra real-id access check as below */
                if (faccessat(relay_basedir_fd, CTL_CHANNEL_NAME, R_OK|W_OK, 0) != 0){
                        close(control_channel);
                        return -5;
                }
                if (control_channel >= 0)
                        goto out; /* It's OK to bypass the [f]access[at] check below,
                                     since this would only occur the *second* time 
                                     staprun tries this gig, or within unprivileged stapio. */
        }
        /* PR14245, NB: we fall through to /sys ... /proc searching,
           in case the relay_basedir_fd option wasn't given (i.e., for
           early in staprun), or if errors out for some reason. */
#endif

	if (statfs("/sys/kernel/debug", &st) == 0 && (int)st.f_type == (int)DEBUGFS_MAGIC) {
                /* PR14245: allow subsequent operations, and if
                   necessary, staprun->stapio forks, to reuse an fd for 
                   directory lookups (even if some parent directories have
                   perms 0700. */
#ifdef HAVE_OPENAT
                if (! sprintf_chk(buf, "/sys/kernel/debug/systemtap/%s", name)) {
                        relay_basedir_fd = open (buf, O_DIRECTORY | O_RDONLY);
                        /* If this fails, we don't much care; the
                           negative return value will just keep us
                           looking up by name again next time. */
                        /* NB: we don't plan to close this fd, so that we can pass
                           it across staprun->stapio fork/execs. */
                }
#endif
		if (sprintf_chk(buf, "/sys/kernel/debug/systemtap/%s/%s", 
                                name, CTL_CHANNEL_NAME))
			return -1;
	} else {
		old_transport = 1;
		if (sprintf_chk(buf, "/proc/systemtap/%s/%s", name, CTL_CHANNEL_NAME))
			return -2;
	}

	control_channel = open(buf, O_RDWR);
	dbug(2, "Opened %s (%d)\n", buf, control_channel);

	/* NB: Even if open() succeeded with effective-UID permissions, we
	 * need the access() check to make sure real-UID permissions are also
	 * sufficient.  When we run under the setuid staprun, effective and
	 * real UID may not be the same.  Specifically, we want to prevent 
         * a local stapusr from trying to attach to a different stapusr's module.
	 *
	 * The access() is done *after* open() to avoid any TOCTOU-style race
	 * condition.  We believe it's probably safe either way, as the file
	 * we're trying to access connot be modified by a typical user, but
	 * better safe than sorry.
	 */
#ifdef HAVE_OPENAT
        if (relay_basedir_fd >= 0) {
                if (faccessat (relay_basedir_fd, CTL_CHANNEL_NAME, R_OK|W_OK, 0) == 0)
                        goto out;
        }
#endif
	if (access(buf, R_OK|W_OK) != 0){
		close(control_channel);
		err("ERROR: no access to debugfs; try \"chmod 0755 %s\" as root\n",
                     DEBUGFSDIR);
		return -5;
	}

out:
	if (control_channel < 0) {
		if (verb) {
			if (attach_mod && errno == ENOENT)
				err(_("ERROR: Can not attach. Module %s not running.\n"), name);
			else
				perr(_("Couldn't open control channel '%s'"), buf);
		}
		return -3;
	}
	if (set_clexec(control_channel) < 0)
		return -4;

	return old_transport;
}

void close_ctl_channel(void)
{
  if (control_channel >= 0) {
          	dbug(2, "Closed ctl fd %d\n", control_channel);
		close(control_channel);
		control_channel = -1;
	}
}
