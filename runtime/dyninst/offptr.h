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

/* Implementation NB: By nature, NULL is never ever a relative pointer, always
 * absolute.  If it's treated as a plain offset like any other pointer, then it
 * will definitely be wrong when the base is changed.  Thus, NULL must be
 * treated as a special case.  */

/* Here we have a few different implementations for testing and comparison:
 *
 *   OFFPTR_IMPL_GLOBAL: Offsets are stored relative to a global pointer, the
 *   shared-memory base.  This means it will only work for pointers within shm.
 *
 *   OFFPTR_IMPL_SELF: Offsets are stored relative to the offptr_t itself.
 *   This has a little more flexibility, but copying offptr_t values requires
 *   more pointer arithmetic.
 *
 *   OFFPTR_IMPL_POINTERS: Plain pointers, not suitable for use where the
 *   relative functionality is actually needed!  (e.g. anything multiprocess)
 *
 * The default for now is OFFPTR_IMPL_GLOBAL.
 */
#if !defined(OFFPTR_IMPL_GLOBAL) \
	&& !defined(OFFPTR_IMPL_SELF) \
	&& !defined(OFFPTR_IMPL_POINTERS)
#define OFFPTR_IMPL_GLOBAL 1
#endif

#if defined(OFFPTR_IMPL_GLOBAL)
/* OFFPTR_IMPL_GLOBAL: In this mode, the offset is stored relative to the
 * shared-memory base pointer (see runtime/dyninst/shm.c).  This has the
 * advantage of very easy offptr_t copies.  The disadvantage is that it only
 * works for pointers that are part of shared memory.  It may also be costly to
 * dereference the global pointer all the time, but that can be measured.
 *
 * For NULL, we can get away with the special-case of offset 0, so long as we
 * accept that the very base of shared memory is not a valid offptr_t target.
 * Given that, offset 0 should be simpler for code gen, and insulates against
 * bugs slightly since a calloced offptr_t will already represent NULL.
 */

typedef struct {
	/* The offset is always relative to the global shared-memory base.
	 * NULL is special-cased as offset==0.  */
	ptrdiff_t offset;
} offptr_t;

static void* _stp_shm_base; /* from runtime/dyninst/shm.c */

static inline void *
offptr_get(offptr_t* op)
{
	return op->offset ? (_stp_shm_base + op->offset) : NULL;
}

static inline void
offptr_set(offptr_t* op, void* ptr)
{
	op->offset = ptr ? (ptr - _stp_shm_base) : 0;
}

#elif defined(OFFPTR_IMPL_SELF)
/* OFFPTR_IMPL_SELF: In this mode, the offset is stored relative to the
 * offptr_t itself.  The advantage of this is better abstraction, as different
 * offptr_t could refer to different memory blocks (as long as they're
 * self-contained).  The disadvantage is that it requires more math to copy
 * offptr_t values around, as with linked-list updates.
 *
 * For NULL, we could let offset 0 be special, but linked lists often want to
 * link back to themselves, which would be a legitimate offset 0.  Instead,
 * we'll let offset 1 represent NULL, which should never happen naturally as
 * it's in the middle of the offptr_t itself.
 */

typedef struct {
	/* The offset is always relative to the offptr_t itself.
	 * NULL is special-cased as offset==1.  */
	ptrdiff_t offset;
} offptr_t;

static inline void *
offptr_get(offptr_t* op)
{
	return (op->offset == 1) ? NULL : (op->offset + (void*)op);
}

static inline void
offptr_set(offptr_t* op, void* ptr)
{
	op->offset = (ptr == NULL) ? 1 : (ptr - (void*)op);
}

#elif defined(OFFPTR_IMPL_POINTERS)
/* OFFPTR_IMPL_POINTERS: In this mode, offptr_t is basically a plain pointer
 * again.  Despite the level of abstraction, it should compile down to the same
 * as using direct pointers would.
 *
 * Since this is always absolute, the NULL caveat doesn't apply.
 */
typedef struct { void* pointer; } offptr_t;
static inline void * offptr_get(offptr_t* op) { return op->pointer; }
static inline void offptr_set(offptr_t* op, void* ptr) { op->pointer = ptr; }

#else
#error "No offptr_t implementation?!"
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
