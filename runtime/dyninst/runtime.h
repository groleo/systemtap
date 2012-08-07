/* main dyninst header file
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_RUNTIME_H_
#define _STAPDYN_RUNTIME_H_

#include <ctype.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loc2c-runtime.h"

#if __WORDSIZE == 64
#define CONFIG_64BIT 1
#endif

#define BITS_PER_LONG __BITS_PER_LONG

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

#include "linux_types.h"


static inline int atomic_add_return(int i, atomic_t *v)
{
	return __sync_add_and_fetch(&(v->counter), i);
}

static inline int atomic_sub_return(int i, atomic_t *v)
{
	return __sync_sub_and_fetch(&(v->counter), i);
}

static inline int pseudo_atomic_cmpxchg(atomic_t *v, int oldval, int newval)
{
	return __sync_val_compare_and_swap(&(v->counter), oldval, newval);
}


static pthread_mutex_t stapdyn_big_dumb_lock = PTHREAD_MUTEX_INITIALIZER;

static inline void preempt_disable(void)
{
	pthread_mutex_lock(&stapdyn_big_dumb_lock);
}

static inline void preempt_enable_no_resched(void)
{
	pthread_mutex_unlock(&stapdyn_big_dumb_lock);
}


#include "linux_defs.h"

#define MODULE_DESCRIPTION(str)
#define MODULE_LICENSE(str)

/* XXX for now, act like uniprocessor... */
#define NR_CPUS 1
#define num_online_cpus() 1
#define smp_processor_id() 0
#define get_cpu() 0
#define put_cpu() 0
#define for_each_possible_cpu(cpu) for ((cpu) = 0; (cpu) < NR_CPUS; ++(cpu))
#define stp_for_each_cpu(cpu) for_each_possible_cpu((cpu))
#define yield() sched_yield()

#define access_ok(type, addr, size) 1

#include "debug.h"

#include "io.c"
#include "alloc.c"
#include "print.h"
#include "stp_string.c"
#include "arith.c"
#include "copy.c"
#include "regs.c"
#include "regs-ia64.c"
#include "task_finder.c"
#include "sym.c"
#include "perf.c"
#include "addr-map.c"
#include "unwind.c"

static int systemtap_module_init(void);
static void systemtap_module_exit(void);

static unsigned long stap_hash_seed; /* Init during module startup */

int stp_dummy_init(void)
{
    stap_hash_seed = _stp_random_u ((unsigned long)-1);
    return systemtap_module_init();
}

int stp_dummy_exit(void)
{
    systemtap_module_exit();
    return 0;
}

#endif /* _STAPDYN_RUNTIME_H_ */
