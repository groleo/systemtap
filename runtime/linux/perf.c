/* -*- linux-c -*- 
 * Perf Functions
 * Copyright (C) 2006-2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _PERF_C_
#define _PERF_C_

#include <linux/perf_event.h>

#include "perf.h"

/** @file perf.c
 * @brief Implements performance monitoring hardware support
 */

/** Initialize performance sampling
 * Call this during probe initialization to set up performance event sampling
 * for all online cpus.  Returns non-zero on error.
 *
 * @param stp Handle for the event to be registered.
 */
static long _stp_perf_init (struct stap_perf_probe *stp, struct task_struct* task)
{
	int cpu;

	if (stp->per_thread) {
	  if (task == 0) /* need to setup later when we know the task */
	    return 0;
	  else  {
	    if (stp->e.t.per_thread_event != 0) /* already setup */
	      return 0;
	    stp->e.t.per_thread_event = perf_event_create_kernel_counter(&stp->attr,
								     -1, task,
								     stp->callback
#ifdef STAPCONF_PERF_COUNTER_CONTEXT
								     , NULL
#endif
								     );
	    if (IS_ERR(stp->e.t.per_thread_event)) {
		long rc = PTR_ERR(stp->e.t.per_thread_event);
		stp->e.t.per_thread_event = NULL;
		return rc;
	      }
	  }
	}
	else {
	  /* allocate space for the event descriptor for each cpu */
	  stp->e.events = _stp_alloc_percpu (sizeof(struct perf_event*));
	  if (stp->e.events == NULL) {
	    return -ENOMEM;
	  }

	  /* initialize event on each processor */
	  for_each_possible_cpu(cpu) {
	    struct perf_event **event = per_cpu_ptr (stp->e.events, cpu);
	    if (cpu_is_offline(cpu)) {
	      *event = NULL;
	      continue;
	    }
	    *event = perf_event_create_kernel_counter(&stp->attr,
						      cpu,
#if defined(STAPCONF_PERF_STRUCTPID) || defined (STAPCONF_PERF_COUNTER_CONTEXT)
						      NULL,
#else
						      -1,
#endif
						      stp->callback
#ifdef STAPCONF_PERF_COUNTER_CONTEXT
						      , NULL
#endif
						      );

	    if (IS_ERR(*event)) {
	      long rc = PTR_ERR(*event);
	      *event = NULL;
	      _stp_perf_del(stp);
	      return rc;
	    }
	  }
	} /* (stp->per_thread) */
	return 0;
}

/** Delete performance event.
 * Call this to shutdown performance event sampling
 *
 * @param stp Handle for the event to be unregistered.
 */
static void _stp_perf_del (struct stap_perf_probe *stp)
{
  int cpu;
  if (! stp || !stp->e.events)
    return;

  /* shut down performance event sampling */
  if (stp->per_thread) {
    if (stp->e.t.per_thread_event) {
      perf_event_release_kernel(stp->e.t.per_thread_event);
    }
    stp->e.t.per_thread_event = NULL;
  }
  else {
    for_each_possible_cpu(cpu) {
      struct perf_event **event = per_cpu_ptr (stp->e.events, cpu);
      if (*event) {
	perf_event_release_kernel(*event);
      }
    }
    _stp_free_percpu (stp->e.events);
    stp->e.events = NULL;
  }
}


/*
The first call to _stp_perf_init, via systemtap_module_init at runtime, is for
setting up aggregate counters.  Per thread counters need to be setup when the
thread is known.  This is done by calling _stp_perf_init later when the thread
is known.  A per thread perf counter is defined by a counter("var") suffix on
the perf probe.  It is defined by perf_builder.  This counter is read on demand 
via the "@perf("var")" builtin which is treated as an expression right hand side
which reads the perf counter associated with the previously defined perf
counter.  It is expanded by dwarf_var_expanding_visitor
*/

static int _stp_perf_read_init (unsigned i, struct task_struct* task)
{
  /* Choose the stap_perf_probes entry */
  struct stap_perf_probe* stp = & stap_perf_probes[i];

  return _stp_perf_init (stp, task);
}


long _stp_perf_read (int ncpu, unsigned i)
{
  /* Choose the stap_perf_probes entry */
  struct stap_perf_probe* stp;
  u64 enabled, running;

  if (i > sizeof(stap_perf_probes)/sizeof(struct stap_perf_probe))
    {
      _stp_error ("_stp_perf_read\n");
      return 0;
    }
  stp = & stap_perf_probes[i]; 
    
  if (stp == NULL || stp->e.t.per_thread_event == NULL)
    {
      _stp_error ("_stp_perf_read\n");
      return 0;
    }

  might_sleep();
  return perf_event_read_value (stp->e.t.per_thread_event, &enabled, &running);

}


#endif /* _PERF_C_ */
