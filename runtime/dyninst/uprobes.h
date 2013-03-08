/* -*- linux-c -*-
 * Common data types for dyninst-based uprobes
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _UPROBES_DYNINST_H_
#define _UPROBES_DYNINST_H_

/* STAPDU: SystemTap Dyninst Uprobes */


/* A stapdu_target describes the files to probe */
struct stapdu_target {
	const char * const path;
	/* buildid? */
};


/* A stapdu_probe has the individual information for a probe.  */
struct stapdu_probe {
	uint64_t target; /* the target index for this probe */
	uint64_t offset; /* the probe offset within the file */
	uint64_t semaphore; /* the sdt semaphore offset within the file */
	uint64_t flags; /* a mask of STAPDYN_PROBE_FLAG_* */
	const struct stap_probe * const probe;
};


#endif /* _UPROBES_DYNINST_H_ */

