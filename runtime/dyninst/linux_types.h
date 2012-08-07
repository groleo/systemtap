/* Types borrowed from the Linux kernel, thus subject to GPLv2. */

#ifndef _STAPDYN_LINUX_TYPES_H_
#define _STAPDYN_LINUX_TYPES_H_


typedef unsigned long long cycles_t;

typedef struct {
        int counter;
} atomic_t;

# define POISON_POINTER_DELTA 0
#define LIST_POISON1  ((void *) 0x00100100 + POISON_POINTER_DELTA)
#define LIST_POISON2  ((void *) 0x00200200 + POISON_POINTER_DELTA)

struct list_head {
	struct list_head *next, *prev;
};

struct hlist_head {
	struct hlist_node *first;
};

struct hlist_node {
	struct hlist_node *next, **pprev;
};

#define PAGE_SHIFT	12
#define PAGE_SIZE	(1UL << PAGE_SHIFT)


#endif /* _STAPDYN_LINUX_TYPES_H_ */
