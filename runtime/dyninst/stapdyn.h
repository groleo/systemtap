/* stapdyn interface header
 * Copyright (C) 2012 Red Hat Inc.
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
 * module, either directly or via dyninst in the mutatee.  */

extern int stp_dyninst_session_init(void);
extern void stp_dyninst_session_exit(void);

extern int enter_dyninst_uprobe(uint64_t index, struct pt_regs *regs);


#ifdef __cplusplus
}
#endif

#endif /* _STAPDYN_H_ */
