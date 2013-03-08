/* stapdyn interface header
 * Copyright (C) 2012-2013 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_H_
#define _STAPDYN_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <asm/ptrace.h>


/* These are declarations of all interfaces that stapdyn may call in the
 * module, either directly or via dyninst in the mutatee.  To maintain
 * compatibility as much as possible, function signatures should not be
 * changed between releases, only deprecated/renamed as necessary.
 */


/* STAP 2.0 : */

extern int stp_dyninst_session_init(void);
extern void stp_dyninst_session_exit(void);

extern uint64_t stp_dyninst_target_count(void);
extern const char* stp_dyninst_target_path(uint64_t index);

extern uint64_t stp_dyninst_probe_count(void);
extern uint64_t stp_dyninst_probe_target(uint64_t index);
extern uint64_t stp_dyninst_probe_offset(uint64_t index);
extern uint64_t stp_dyninst_probe_semaphore(uint64_t index);

extern int enter_dyninst_uprobe(uint64_t index, struct pt_regs *regs);

/* This is somewhat of a hack until we can figure out how to build a pt_regs
 * struct directly with stapdyn.  The varargs are all unsigned long, giving
 * first the original PC, then DWARF-ordered registers.  */
extern int enter_dyninst_uprobe_regs(uint64_t index, unsigned long nregs, ...);

/* STAP 2.x : */

/* uprobes-like flags */
#define STAPDYN_PROBE_FLAG_RETURN	0x1

/* utrace-like flags */
#define STAPDYN_PROBE_FLAG_PROC_BEGIN	0x100
#define STAPDYN_PROBE_FLAG_PROC_END	0x200
#define STAPDYN_PROBE_FLAG_THREAD_BEGIN	0x400
#define STAPDYN_PROBE_FLAG_THREAD_END	0x800

extern uint64_t stp_dyninst_probe_flags(uint64_t index);

extern int enter_dyninst_utrace_probe(uint64_t index, struct pt_regs *regs);

extern const char* stp_dyninst_shm_init(void);
extern int stp_dyninst_shm_connect(const char* name);


#define STAPDYN_PROBE_ALL_FLAGS (uint64_t)(STAPDYN_PROBE_FLAG_RETURN	\
    | STAPDYN_PROBE_FLAG_PROC_BEGIN | STAPDYN_PROBE_FLAG_PROC_END	\
    | STAPDYN_PROBE_FLAG_THREAD_BEGIN | STAPDYN_PROBE_FLAG_THREAD_END)

#ifdef __cplusplus
}
#endif

#endif /* _STAPDYN_H_ */
