/* -*- linux-c -*-
 * Common functions for using dyninst-based uprobes
 * Copyright (C) 2012-2013 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _UPROBES_DYNINST_C_
#define _UPROBES_DYNINST_C_

/* STAPDU: SystemTap Dyninst Uprobes */

// loc2c-generated code assumes pt_regs are available, so use this to make
// sure we always have *something* for it to dereference...
static struct pt_regs stapdu_dummy_uregs = {0};

/* These functions implement the ABI in stapdyn.h
 *
 * NB: The translator will generate two arrays used here:
 *   struct stapdu_target stapdu_targets[];
 *   struct stapdu_probe stapdu_probes[];
 */


uint64_t stp_dyninst_target_count(void)
{
	return ARRAY_SIZE(stapdu_targets);
}

const char* stp_dyninst_target_path(uint64_t index)
{
	if (index >= stp_dyninst_target_count())
		return NULL;
	return stapdu_targets[index].path;
}


uint64_t stp_dyninst_probe_count(void)
{
	return ARRAY_SIZE(stapdu_probes);
}

uint64_t stp_dyninst_probe_target(uint64_t index)
{
	if (index >= stp_dyninst_probe_count())
		return (uint64_t)-1;
	return stapdu_probes[index].target;
}

uint64_t stp_dyninst_probe_offset(uint64_t index)
{
	if (index >= stp_dyninst_probe_count())
		return (uint64_t)-1;
	return stapdu_probes[index].offset;
}

uint64_t stp_dyninst_probe_semaphore(uint64_t index)
{
	if (index >= stp_dyninst_probe_count())
		return (uint64_t)-1;
	return stapdu_probes[index].semaphore;
}

uint64_t stp_dyninst_probe_flags(uint64_t index)
{
	if (index >= stp_dyninst_probe_count())
		return (uint64_t)-1;
	return stapdu_probes[index].flags;
}

int enter_dyninst_uprobe(uint64_t index, struct pt_regs *regs)
	__attribute__((weak));

int enter_dyninst_uprobe_regs(uint64_t index, unsigned long nregs, ...)
{
	struct pt_regs regs = {0};

	va_list varegs;
	va_start(varegs, nregs);

#ifdef __i386__
	// XXX Dyninst currently has a bug where it's only passing a 32-bit
	// index, which means nregs gets stuffed into the upper bits of index,
	// and the varegs are all off by one.  Hacking it into shape for now...
	if (index > UINT32_MAX) {
		SET_REG_IP((&regs), nregs);
                nregs = index >> 32;
                index &= UINT32_MAX;
        } else
#endif
	if (nregs > 0)
		SET_REG_IP((&regs), va_arg(varegs, unsigned long));

	/* NB: pt_regs_store_register() expects literal register numbers to
	 * paste as CPP tokens, so unfortunately this has to be unrolled.  */
#define SET_REG(n) if (n < nregs - 1) \
			pt_regs_store_register((&regs), n, \
					va_arg(varegs, unsigned long))
#if defined(__i386__) || defined(__x86_64__)
	SET_REG(0);
	SET_REG(1);
	SET_REG(2);
	SET_REG(3);
	SET_REG(4);
	SET_REG(5);
	SET_REG(6);
	SET_REG(7);
#endif
#if defined(__x86_64__)
	SET_REG(8);
	SET_REG(9);
	SET_REG(10);
	SET_REG(11);
	SET_REG(12);
	SET_REG(13);
	SET_REG(14);
	SET_REG(15);
#endif
#undef SET_REG

	va_end(varegs);

	if (enter_dyninst_uprobe)
		return enter_dyninst_uprobe(index, &regs);
	return -1;
}

#endif /* _UPROBES_DYNINST_C_ */

