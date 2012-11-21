/* pointers based on relative offsets, for shared memory
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _OFFPTR_H
#define _OFFPTR_H


/* An offset pointer refers to memory without using an absolute address.  This
 * is useful for shared memory between processes, and perhaps also for cases
 * where memory may move, as with realloc.  */


#ifndef OFFPTR_DEBUG_DIRECT

typedef struct {
	/* The offset is always relative to the offptr_t itself.  */
	ptrdiff_t offset;
} offptr_t;

static inline void *
offptr_get(offptr_t* op)
{
	return op->offset + (void*)op;
}

static inline void
offptr_set(offptr_t* op, void* ptr)
{
	op->offset = ptr - (void*)op;
}

#else
/* OFFPTR_DEBUG_DIRECT: In this mode, offptr_t is basically a plain pointer
 * again.  Despite the level of abstraction, it should compile down to the same
 * as using direct pointers would.
 */
typedef struct { void* pointer; } offptr_t;
static inline void * offptr_get(offptr_t* op) { return op->pointer; }
static inline void offptr_set(offptr_t* op, void* ptr) { op->pointer = ptr; }
#endif


/* Since offptr_t is untyped, this template-like macro lets you define
 * accessors with the appropriate pointer types enforced.  */
#define DEFINE_OFFPTR_GETSET(prefix, T1, T2, member)			\
	static inline T2* prefix##_##member(T1* ptr) {			\
		return (T2*) offptr_get(&ptr->member);			\
	}								\
	static inline void prefix##_set_##member(T1* ptr, T2* val) {	\
		offptr_set(&ptr->member, val);				\
	}


#endif /* _OFFPTR_H */
