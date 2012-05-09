/* -*- linux-c -*-
 * Common functions for using dyninst-based uprobes
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _UPROBES_DYNINST_C_
#define _UPROBES_DYNINST_C_

/* STAPDU: SystemTap Dyninst Uprobes */

/* A target has the dyninst view of what we want to probe.
 *
 * NB: This becomes an ABI in the module with stapdyn, so be cautious.  It
 * might be better to go with a more flexible format - perhaps JSON?
 */
struct stapdu_target {
	char filename[240];
	uint64_t offset; /* the probe offset within the file */
	uint64_t sdt_sem_offset; /* the semaphore offset from process->base */
};


/* A consumer has the runtime information for a probe.  */
struct stapdu_consumer {
	struct stap_probe * const probe;
};


#endif /* _UPROBES_DYNINST_C_ */

