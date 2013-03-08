/* -*- linux-c -*- 
 * Functions to access the members of pt_regs struct
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _DYNINST_REGS_C_
#define _DYNINST_REGS_C_

// NB: these differ from kernel mode because there's no access
// to eflags, segment, or cr registers.

#if defined  (__x86_64__)

#define EREG(nm, regs) ((regs)->r##nm)
#define RREG(nm, regs) ((regs)->r##nm)

static void _stp_print_regs(struct pt_regs * regs)
{
	_stp_printf("RIP: %016lx\nRSP: %016lx\n",
			RREG(ip, regs), RREG(sp, regs));
	_stp_printf("RAX: %016lx RBX: %016lx RCX: %016lx\n",
			RREG(ax, regs), RREG(bx, regs), RREG(cx, regs));
	_stp_printf("RDX: %016lx RSI: %016lx RDI: %016lx\n",
			RREG(dx, regs), RREG(si, regs), RREG(di, regs));
	_stp_printf("RBP: %016lx R08: %016lx R09: %016lx\n",
			RREG(bp, regs), regs->r8, regs->r9);
	_stp_printf("R10: %016lx R11: %016lx R12: %016lx\n",
			regs->r10, regs->r11, regs->r12);
	_stp_printf("R13: %016lx R14: %016lx R15: %016lx\n",
			regs->r13, regs->r14, regs->r15);
}

#elif defined (__i386__)

#define EREG(nm, regs) ((regs)->e##nm)
#define XREG(nm, regs) ((regs)->x##nm)

/** Write the registers to a string.
 * @param regs The pt_regs saved by the kprobe.
 * @note i386 and x86_64 only so far. 
 */
static void _stp_print_regs(struct pt_regs * regs)
{
	_stp_printf ("EIP: %08lx\n", EREG(ip, regs));
	_stp_printf ("ESP: %08lx\n", EREG(sp, regs));
	_stp_printf ("EAX: %08lx EBX: %08lx ECX: %08lx EDX: %08lx\n",
			EREG(ax, regs), EREG(bx, regs), EREG(cx, regs), EREG(dx, regs));
	_stp_printf ("ESI: %08lx EDI: %08lx EBP: %08lx\n",
			EREG(si, regs), EREG(di, regs), EREG(bp, regs));
}

#endif

#endif /* _DYNINST_REGS_C_ */
