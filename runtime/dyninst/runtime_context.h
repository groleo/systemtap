/* -*- linux-c -*- 
 * Context Runtime Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_RUNTIME_CONTEXT_H_
#define _STAPDYN_RUNTIME_CONTEXT_H_

#include <stdlib.h>
#include <unistd.h>
#include <sched.h>
#include <pthread.h>

/* Defined after stap_probes[] by translate.cxx */
static const char* stp_probe_point(size_t index);

/* Defined later in common_session_state.h */
static inline struct context* stp_session_context(size_t index);

static int _stp_runtime_num_contexts;

/* Locally-cached context pointer -- see _stp_runtime_entryfn_get_context()
 * and _stp_runtime_entryfn_put_context().  */
static __thread struct context *contexts;

static int _stp_runtime_contexts_init(void)
{
    _stp_runtime_num_contexts = sysconf(_SC_NPROCESSORS_ONLN);
    if (_stp_runtime_num_contexts < 1)
	_stp_runtime_num_contexts = 1;
    return 0;
}

static int _stp_runtime_contexts_alloc(void)
{
    int i;

    /* The allocation was already done in stp_session_init;
     * we just need to initialize the context data.  */
    for (i = 0; i < _stp_runtime_num_contexts; i++) {
	int rc;
	struct context *c = stp_session_context(i);
	rc = stp_pthread_mutex_init_shared(&c->lock);
	if (rc != 0) {
	    _stp_error("context mutex initialization failed");
	    return rc;
	}
    }
    return 0;
}

static void _stp_runtime_contexts_free(void)
{
    int i;

    /* The context memory is managed elsewhere;
     * we just need to teardown the context locks. */
    for (i = 0; i < _stp_runtime_num_contexts; i++) {
	struct context *c = stp_session_context(i);
	(void)pthread_mutex_destroy(&c->lock);
    }
}

static int _stp_runtime_get_data_index(void)
{
    int data_index;

    /* If this thread has already gotten a context structure,
     * return the data index from it. */
    if (contexts != NULL)
	return contexts->data_index;

    /* This shouldn't happen. */
    /* FIXME: assert? */
    return 0;
}

static struct context * _stp_runtime_entryfn_get_context(void)
{
    struct context *c;
    int i, index, rc, data_index;

    /* If 'contexts' (which is thread-local storage) is already set
     * for this thread, we are re-entrant, so just quit. */
    if (contexts != NULL)
	return NULL;

    /* Figure out with cpu we're on, which is our default
     * data_index. Make sure the returned data index number is within
     * the range of [0.._stp_runtime_num_contexts]. Be sure to handle
     * a sched_getcpu() failure (it will return -1). */
    data_index = sched_getcpu() % _stp_runtime_num_contexts;
    if (unlikely(data_index < 0))
	data_index = 0;

    /* Try to find a free context structure. */
    index = data_index;
    for (i = 0; i < _stp_runtime_num_contexts; i++, index++) {
	if (index >= _stp_runtime_num_contexts)
	    index = 0;
	c = stp_session_context(index);
	if (pthread_mutex_trylock(&c->lock) == 0) {
	    /* We found a free context structure. Now that it is
	     * locked, set the TLS pointer and return the context. */
	    contexts = c;
	    return contexts;
	}
    }

    /* If we're here, we couldn't find a free context structure. Wait
     * on one. */
    c = stp_session_context(data_index);
    rc = pthread_mutex_lock(&c->lock);
    if (rc == 0) {
	contexts = c;
	return contexts;
    }
    return NULL;
}

static void _stp_runtime_entryfn_put_context(void)
{
    if (contexts) {
	struct context *c = contexts;
	contexts = NULL;
	pthread_mutex_unlock(&c->lock);
    }
    return;
}

static struct context *_stp_runtime_get_context(void)
{
    /* Note we don't call _stp_runtime_entryfn_get_context()
     * here. This function is called after
     * _stp_runtime_entryfn_get_context() and has no corresponding
     * "put" function. */
    return contexts;
}

static void _stp_runtime_context_wait(void)
{
    struct timespec hold_start;
    int hold_index;
    int holdon;

    (void)clock_gettime(CLOCK_MONOTONIC_RAW, &hold_start);
    hold_index = -1;
    do {
	int i;
	holdon = 0;
	struct timespec now, elapsed;
		
	for (i = 0; i < _stp_runtime_num_contexts; i++) {
	    struct context *c = stp_session_context(i);
	    if (atomic_read (&c->busy)) {
		holdon = 1;

		/* Just In case things are really stuck, let's print
		 * some diagnostics. */
		(void)clock_gettime(CLOCK_MONOTONIC_RAW, &now);
		_stp_timespec_sub(&now, &hold_start, &elapsed);

		/* If its been > 1 second since we started and we
		 * haven't already printed a message for this stuck
		 * context, print one. */
		if (elapsed.tv_sec > 0 && (i > hold_index)) {
		    /* NB: c->probe_point is local memory, invalid across processes,
		     * so read it indirectly through c->probe_index instead.  */
		    const char* pp = stp_probe_point(c->probe_index);
		    if (pp)
			_stp_error("context[%d] stuck: %s", i, pp);
		    else
			_stp_error("context[%d] stuck in unknown probe %zu",
				   i, c->probe_index);
		    hold_index = i;
		}
	    }
	}

#ifdef STAP_OVERRIDE_STUCK_CONTEXT
	/* In case things are really really stuck, we are going to
	 * pretend/assume/hope everything is OK, and let the cleanup
	 * finish. */
	(void)clock_gettime(CLOCK_MONOTONIC_RAW, &now);
	_stp_timespec_sub(&now, &hold_start, &elapsed);
	if (elapsed.tv_sec > 10) {
	    _stp_warn("overriding stuck context to allow shutdown.");
	    holdon = 0;			/* allow loop to exit */
	}
#endif

	if (holdon) {
	    sched_yield();
	}
    } while (holdon);
}

#endif /* _STAPDYN_RUNTIME_CONTEXT_H_ */
