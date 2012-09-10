/* Definitions borrowed from the Linux kernel, thus subject to GPLv2. */

#ifndef _STAPDYN_LINUX_DEFS_H_
#define _STAPDYN_LINUX_DEFS_H_

#include <unistd.h>

#include "linux_hash.h"

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

static inline size_t strlcat(char *dest, const char *src, size_t count)
{
	size_t dsize = strlen(dest);
	size_t len = strlen(src);
	size_t res = dsize + len;

	/* This would be a bug */
	//BUG_ON(dsize >= count);
        dsize = min(dsize, count);

	dest += dsize;
	count -= dsize;
	if (len >= count)
		len = count-1;
	memcpy(dest, src, len);
	dest[len] = 0;
	return res;
}

#define __must_check 		__attribute__((warn_unused_result))
# define __force
# define __user
# define __chk_user_ptr(x) (void)0
#define user_mode(regs) 1
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

static int _stp_mem_fd;

static inline __must_check long __copy_from_user(void *to,
		const void __user * from, unsigned long n)
{
	int rc = 0;

	/*
	 * The pread syscall is faster than lseek()/read() (since it
	 * is only one syscall). Also, if we used lseek()/read() we
	 * couldn't use a cached fd - since 2 threads might hit this
	 * code at the same time and the 2nd lseek() might finish
	 * before the 1st read()...
	 */
	if (pread(_stp_mem_fd, to, n, (off_t)from) != n)
		rc = -EFAULT;
	return rc;
}

static inline int __get_user_fn(size_t size, const void __user *ptr, void *x)
{
	size = __copy_from_user(x, ptr, size);
	return size ? -EFAULT : size;
}

extern int __get_user_bad(void) __attribute__((noreturn));

#define __put_user(x, ptr)						\
({									\
	int __gu_err = -EFAULT;						\
	__chk_user_ptr(ptr);						\
	switch (sizeof(*(ptr))) {					\
	case 1: {							\
		unsigned char __x = (unsigned char)(x);			\
		__gu_err = __put_user_fn(sizeof (*(ptr)),		\
					 ptr, &__x);			\
		break;							\
	};								\
	case 2: {							\
		unsigned short __x = (unsigned short)(x);		\
		__gu_err = __put_user_fn(sizeof (*(ptr)),		\
					 ptr, &__x);			\
		break;							\
	};								\
	case 4: {							\
		unsigned int __x = (unsigned int)(x);			\
		__gu_err = __put_user_fn(sizeof (*(ptr)),		\
					 ptr, &__x);			\
		break;							\
	};								\
	case 8: {							\
		unsigned long long __x = (unsigned long long)(x);	\
		__gu_err = __put_user_fn(sizeof (*(ptr)),		\
					 ptr, &__x);			\
		break;							\
	};								\
	default:							\
		__put_user_bad();					\
		break;							\
	}								\
	__gu_err;							\
})

static inline __must_check long __copy_to_user(void *to, const void *from,
					       unsigned long n)
{
	int rc = 0;

	/*
	 * The pwrite syscall is faster than lseek()/write() (since it
	 * is only one syscall).
	 */
	if (pwrite(_stp_mem_fd, to, n, (off_t)from) != n)
		rc = -EFAULT;
	return rc;
}

static inline int __put_user_fn(size_t size, const void __user *ptr, void *x)
{
	size = __copy_to_user(x, ptr, size);
	return size ? -EFAULT : size;
}

extern int __put_user_bad(void) __attribute__((noreturn));

static inline void INIT_LIST_HEAD(struct list_head *list)
{
	list->next = list;
	list->prev = list;
}

static inline void __list_add(struct list_head *new,
			      struct list_head *prev,
			      struct list_head *next)
{
	next->prev = new;
	new->next = next;
	new->prev = prev;
	prev->next = new;
}

static inline void list_add(struct list_head *new, struct list_head *head)
{
	__list_add(new, head, head->next);
}

static inline void list_add_tail(struct list_head *new, struct list_head *head)
{
	__list_add(new, head->prev, head);
}

static inline void __list_del(struct list_head * prev, struct list_head * next)
{
	next->prev = prev;
	prev->next = next;
}

static inline void __list_del_entry(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

static inline void list_del(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

static inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
	__list_del_entry(list);
	list_add_tail(list, head);
}

static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

#define list_for_each_safe(pos, n, head) \
	for (pos = (head)->next, n = pos->next; pos != (head); \
		pos = n, n = pos->next)

static inline void INIT_HLIST_NODE(struct hlist_node *h)
{
	h->next = NULL;
	h->pprev = NULL;
}

static inline int hlist_unhashed(const struct hlist_node *h)
{
	return !h->pprev;
}

static inline void __hlist_del(struct hlist_node *n)
{
	struct hlist_node *next = n->next;
	struct hlist_node **pprev = n->pprev;
	*pprev = next;
	if (next)
		next->pprev = pprev;
}

static inline void hlist_del_init(struct hlist_node *n)
{
	if (!hlist_unhashed(n)) {
		__hlist_del(n);
		INIT_HLIST_NODE(n);
	}
}

static inline void hlist_add_head(struct hlist_node *n, struct hlist_head *h)
{
	struct hlist_node *first = h->first;
	n->next = first;
	if (first)
		first->pprev = &n->next;
	h->first = n;
	n->pprev = &h->first;
}

#define hlist_for_each(pos, head) \
	for (pos = (head)->first; pos ; pos = pos->next)


#endif /* _STAPDYN_LINUX_DEFS_H_ */

