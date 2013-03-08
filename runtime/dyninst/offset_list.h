/* linked lists based on offsets, suitable for shared memory
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _OFFSET_LIST_H
#define _OFFSET_LIST_H

#include "offptr.h"


struct olist_head {
	offptr_t onext, oprev;
};
DEFINE_OFFPTR_GETSET(olist, struct olist_head, struct olist_head, onext);
DEFINE_OFFPTR_GETSET(olist, struct olist_head, struct olist_head, oprev);

static inline void INIT_OLIST_HEAD(struct olist_head *list)
{
	olist_set_onext(list, list);
	olist_set_oprev(list, list);
}

static inline void __olist_add(struct olist_head *new,
			      struct olist_head *prev,
			      struct olist_head *next)
{
	olist_set_oprev(next, new);
	olist_set_onext(new, next);
	olist_set_oprev(new, prev);
	olist_set_onext(prev, new);
}

static inline void olist_add(struct olist_head *new, struct olist_head *head)
{
	__olist_add(new, head, olist_onext(head));
}

static inline void olist_add_tail(struct olist_head *new, struct olist_head *head)
{
	__olist_add(new, olist_oprev(head), head);
}

static inline void __olist_del(struct olist_head * prev, struct olist_head * next)
{
	olist_set_oprev(next, prev);
	olist_set_onext(prev, next);
}

static inline void __olist_del_entry(struct olist_head *entry)
{
	__olist_del(olist_oprev(entry), olist_onext(entry));
}

static inline void olist_del(struct olist_head *entry)
{
	__olist_del(olist_oprev(entry), olist_onext(entry));
	olist_set_onext(entry, LIST_POISON1);
	olist_set_oprev(entry, LIST_POISON2);
}

static inline void olist_move_tail(struct olist_head *list, struct olist_head *head)
{
	__olist_del_entry(list);
	olist_add_tail(list, head);
}

static inline int olist_empty(struct olist_head *head)
{
	return olist_onext(head) == head;
}

#define olist_entry(ptr, type, member)	\
	container_of(ptr, type, member)

#define olist_for_each_safe(pos, n, head)			\
	for (pos = olist_onext(head), n = olist_onext(pos);	\
	     pos != (head); pos = n, n = olist_onext(pos))

#define olist_for_each_entry(pos, head, member)					\
	for (pos = olist_entry(olist_onext(head), typeof(*pos), member);		\
	     &pos->member != (head);						\
	     pos = olist_entry(olist_onext(&pos->member), typeof(*pos), member))

#define olist_for_each_entry_safe(pos, n, head, member)				\
	for (pos = olist_entry(olist_onext(head), typeof(*pos), member),		\
	     n = olist_entry(olist_onext(&pos->member), typeof(*pos), member);	\
	     &pos->member != (head);						\
	     pos = n, n = olist_entry(olist_onext(&n->member), typeof(*n), member))



struct ohlist_head {
	offptr_t ofirst;
};
struct ohlist_node {
	offptr_t onext, opprev;
};
DEFINE_OFFPTR_GETSET(ohlist, struct ohlist_head, struct ohlist_node, ofirst);
DEFINE_OFFPTR_GETSET(ohlist, struct ohlist_node, struct ohlist_node, onext);
DEFINE_OFFPTR_GETSET(ohlist, struct ohlist_node, offptr_t, opprev);

static inline void INIT_OHLIST_HEAD(struct ohlist_head *head)
{
	ohlist_set_ofirst(head, NULL);
}

static inline void INIT_OHLIST_NODE(struct ohlist_node *node)
{
	ohlist_set_onext(node, NULL);
	ohlist_set_opprev(node, NULL);
}

static inline int ohlist_unhashed(struct ohlist_node *node)
{
	return !ohlist_opprev(node);
}

static inline void __ohlist_del(struct ohlist_node *node)
{
	struct ohlist_node *next = ohlist_onext(node);
	offptr_t *pprev = ohlist_opprev(node);
	offptr_set(pprev, next);
	if (next)
		ohlist_set_opprev(next, pprev);
}

static inline void ohlist_del_init(struct ohlist_node *node)
{
	if (!ohlist_unhashed(node)) {
		__ohlist_del(node);
		INIT_OHLIST_NODE(node);
	}
}

static inline void ohlist_add_head(struct ohlist_node *node, struct ohlist_head *head)
{
	struct ohlist_node *first = ohlist_ofirst(head);
	ohlist_set_onext(node, first);
	if (first)
		ohlist_set_opprev(first, &node->onext);
	ohlist_set_ofirst(head, node);
	ohlist_set_opprev(node, &head->ofirst);
}

#define ohlist_entry(ptr, type, member) \
	container_of(ptr, type, member)

#define ohlist_for_each(pos, head) \
	for (pos = ohlist_ofirst(head); pos ; pos = ohlist_onext(pos))

#define ohlist_for_each_entry(tpos, pos, head, member)				\
	for (pos = ohlist_ofirst(head);						\
	     pos && ({ tpos = ohlist_entry(pos, typeof(*tpos), member); 1;});	\
	     pos = ohlist_onext(pos))


#endif /* _OFFSET_LIST_H */
