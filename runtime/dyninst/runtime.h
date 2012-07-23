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

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

/********** shamelessly stolen kernel types **********/

typedef unsigned long long cycles_t;

typedef struct {
        int counter;
} atomic_t;

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1UL << PAGE_SHIFT)

/********** end stolen types **********/


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


/********** shamelessly stolen kernel functions **********/

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

#define min_t(type, x, y) ({			\
	type __min1 = (x);			\
	type __min2 = (y);			\
	__min1 < __min2 ? __min1: __min2; })

#define max_t(type, x, y) ({			\
	type __max1 = (x);			\
	type __max2 = (y);			\
	__max1 > __max2 ? __max1: __max2; })

#define clamp(val, min, max) ({			\
	typeof(val) __val = (val);		\
	typeof(min) __min = (min);		\
	typeof(max) __max = (max);		\
	(void) (&__val == &__min);		\
	(void) (&__val == &__max);		\
	__val = __val < __min ? __min: __val;	\
	__val > __max ? __max: __val; })

#define likely(x)      __builtin_expect(!!(x), 1)
#define unlikely(x)    __builtin_expect(!!(x), 0)

#define __must_be_array(arr) 0
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))

#define ATOMIC_INIT(i)  { (i) }

static inline void atomic_inc(atomic_t *v)
{
	atomic_add_return(1, v);
}

static inline void atomic_dec(atomic_t *v)
{
	atomic_sub_return(1, v);
}

static inline int atomic_read(const atomic_t *v)
{
	return (*(volatile int *)&(v)->counter);
}

static inline void atomic_set(atomic_t *v, int i)
{
	v->counter = i;
}

#define atomic_inc_return(v)		atomic_add_return(1, (v))


#define do_div(n,base) ({					\
	uint32_t __base = (base);				\
	uint32_t __rem;						\
	__rem = ((uint64_t)(n)) % __base;			\
	(n) = ((uint64_t)(n)) / __base;				\
	__rem;							\
 })

static inline size_t strlcpy(char *dest, const char *src, size_t size)
{
	size_t ret = strlen(src);

	if (size) {
		size_t len = (ret >= size) ? size - 1 : ret;
		memcpy(dest, src, len);
		dest[len] = '\0';
	}
	return ret;
}

#define __must_check 		__attribute__((warn_unused_result))
# define __force
# define __user
# define __chk_user_ptr(x) (void)0
#define __get_user(x, ptr)					\
({								\
	int __gu_err = -EFAULT;					\
	__chk_user_ptr(ptr);					\
	switch (sizeof(*(ptr))) {				\
	case 1: {						\
		unsigned char __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 2: {						\
		unsigned short __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 4: {						\
		unsigned int __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	case 8: {						\
		unsigned long long __x;				\
		__gu_err = __get_user_fn(sizeof (*(ptr)),	\
					 ptr, &__x);		\
		(x) = *(__force __typeof__(*(ptr)) *) &__x;	\
		break;						\
	};							\
	default:						\
		__get_user_bad();				\
		break;						\
	}							\
	__gu_err;						\
})

static inline __must_check long __copy_from_user(void *to,
		const void __user * from, unsigned long n)
{
	if (__builtin_constant_p(n)) {
		switch(n) {
		case 1:
			*(u8 *)to = *(u8 __force *)from;
			return 0;
		case 2:
			*(u16 *)to = *(u16 __force *)from;
			return 0;
		case 4:
			*(u32 *)to = *(u32 __force *)from;
			return 0;
#ifdef CONFIG_64BIT
		case 8:
			*(u64 *)to = *(u64 __force *)from;
			return 0;
#endif
		default:
			break;
		}
	}

	memcpy(to, (const void __force *)from, n);
	return 0;
}

static inline int __get_user_fn(size_t size, const void __user *ptr, void *x)
{
	size = __copy_from_user(x, ptr, size);
	return size ? -EFAULT : size;
}

extern int __get_user_bad(void) __attribute__((noreturn));

/********** end stolen functions **********/


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

int stp_dummy_init(void)
{
    return systemtap_module_init();
}

int stp_dummy_exit(void)
{
    systemtap_module_exit();
    return 0;
}

#endif /* _STAPDYN_RUNTIME_H_ */
