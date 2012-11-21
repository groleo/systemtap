/* -*- linux-c -*- 
 * Map list abstractions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _STAPDYN_MAP_LIST_H_
#define _STAPDYN_MAP_LIST_H_

#include "offset_list.h"

#define mlist_head	olist_head
#define mlist_next	olist_onext
#define mlist_prev	olist_oprev

#define INIT_MLIST_HEAD	INIT_OLIST_HEAD

#define mlist_add	olist_add
#define mlist_del	olist_del
#define mlist_empty	olist_empty
#define mlist_entry	olist_entry
#define mlist_move_tail	olist_move_tail

#define mlist_for_each_safe	olist_for_each_safe


#define mhlist_head	ohlist_head
#define mhlist_node	ohlist_node

#define INIT_MHLIST_HEAD	INIT_OHLIST_HEAD
#define INIT_MHLIST_NODE	INIT_OHLIST_NODE

#define mhlist_add_head	ohlist_add_head
#define mhlist_del_init	ohlist_del_init

#define mhlist_for_each_entry	ohlist_for_each_entry


#endif /* _STAPDYN_MAP_LIST_H_ */
