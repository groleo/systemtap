/* -*- linux-c -*- 
 * Memory allocation functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_ALLOC_C_
#define _STAPDYN_ALLOC_C_

#include <stdlib.h>


#define STP_ALLOC_FLAGS 0
#define STP_ALLOC_SLEEP_FLAGS 0


#define _stp_kmalloc(size) malloc((size))
#define _stp_kmalloc_gfp(size, gfp_mask) malloc((size))

#define _stp_kzalloc(size) calloc((size), 1)
#define _stp_kzalloc_gfp(size, gfp_mask) calloc((size), 1)

#define _stp_kfree(addr) free(addr)


/* XXX for now, act like uniprocessor... */
#define _stp_alloc_percpu(size) calloc((size), 1)
#define _stp_free_percpu(addr) free((addr))
#define per_cpu_ptr(ptr, cpu) (ptr)


/* XXX for now, we're NUMA unaware */
#define _stp_kmalloc_node(size, node) malloc((size))
#define _stp_kmalloc_node_gfp(size, node, gfp_mask) malloc((size))

#define _stp_kzalloc_node(size, node) calloc((size), 1)
#define _stp_kzalloc_node_gfp(size, node, gfp_mask) calloc((size), 1)


#endif /* _STAPDYN_ALLOC_C_ */

