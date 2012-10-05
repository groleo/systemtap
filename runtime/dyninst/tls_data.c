/* -*- linux-c -*- 
 * TLS Data Functions
 * Copyright (C) 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

#ifndef _TLS_DATA_C_
#define _TLS_DATA_C_

#include <pthread.h>
#include <errno.h>

struct tls_data_object_t;

struct tls_data_container_t {
	pthread_key_t key;		/* key indexing TLS objects */
	size_t size;			/* allocated size of a new TLS object */
	struct list_head head;		/* list of tls_data_object_t structs */
	pthread_rwlock_t lock;		/* lock protecting list */
	int (*init_function)(struct tls_data_object_t *);
	void (*free_function)(struct tls_data_object_t *);
};

struct tls_data_object_t {
	struct list_head list;
	struct tls_data_container_t *container;
};

#ifdef STP_DEBUG_CONTAINER_LOCK
#define TLS_DATA_CONTAINER_INIT(con) pthread_rwlock_init(&(con)->lock, NULL)
#define TLS_DATA_CONTAINER_LOCK(con) {printf("%s:%d rd locking %p\n", __FUNCTION__, __LINE__, &(con)->lock); pthread_rwlock_rdlock(&(con)->lock);}
#define TLS_DATA_CONTAINER_WRLOCK(con) {printf("%s:%d wr locking %p\n", __FUNCTION__, __LINE__, &(con)->lock); pthread_rwlock_wrlock(&(con)->lock);}
#define TLS_DATA_CONTAINER_UNLOCK(con) {pthread_rwlock_unlock(&(con)->lock); printf("%s:%d unlocked %p\n", __FUNCTION__, __LINE__, &(con)->lock); }
#define TLS_DATA_CONTAINER_DESTROY(con) (void)pthread_rwlock_destroy(&(con)->lock)
#else
#define TLS_DATA_CONTAINER_INIT(con) pthread_rwlock_init(&(con)->lock, NULL)
#define TLS_DATA_CONTAINER_LOCK(con) pthread_rwlock_rdlock(&(con)->lock)
#define TLS_DATA_CONTAINER_WRLOCK(con) pthread_rwlock_wrlock(&(con)->lock)
#define TLS_DATA_CONTAINER_UNLOCK(con) pthread_rwlock_unlock(&(con)->lock)
#define TLS_DATA_CONTAINER_DESTROY(con) (void)pthread_rwlock_destroy(&(con)->lock)
#endif

#define for_each_tls_data(obj, container) \
	list_for_each_entry((obj), &(container)->head, list)

#define for_each_tls_data_safe(obj, n, container)		\
	list_for_each_entry_safe((obj), (n), &(container)->head, list)

static void _stp_tls_free_per_thread_ptr(void *addr)
{
	struct tls_data_object_t *obj = addr;

	if (obj != NULL) {
		struct tls_data_container_t *container = obj->container;

		/* Remove this object from the container's list of objects */
		if (container) {
			TLS_DATA_CONTAINER_WRLOCK(container);
			list_del(&obj->list);
			TLS_DATA_CONTAINER_UNLOCK(container);

			/* Give the code above us a chance to cleanup. */
			if (container->free_function)
				container->free_function(obj);
		}

		/* Note that this free() call only works correctly if
		 * the struct tls_data_object is the first thing in
		 * its containing structure. */
		free(obj);
	}
}

static struct tls_data_object_t *
_stp_tls_get_per_thread_ptr(struct tls_data_container_t *container)
{
	/* See if we've already got an object for this thread. */
	struct tls_data_object_t *obj = pthread_getspecific(container->key);

	/* If we haven't set up an tls object instance for the key for
	 * this thread yet, allocate one. */
	if (obj == NULL) {
		int rc;

		/* The real alloc_percpu() allocates zero-filled
		 * memory, so we need to so the same. */
		obj = calloc(container->size, 1);
		if (obj == NULL) {
			_stp_error("Couldn't allocate tls object memory: %d\n",
				   errno);
			goto exit;
		}

		/* Give the code above us a chance to initialize the
		 * newly created object. */
		obj->container = container;
		if (container->init_function) {
			if (container->init_function(obj) != 0) {
				free(obj);
				obj = NULL;
				goto exit;
			}
		}

		/* Inform pthreads about this instance. */
		TLS_DATA_CONTAINER_WRLOCK(container);
		if ((rc = pthread_setspecific(container->key, obj)) == 0) {
			/* Add obj to container's list of objs (for
			 * use in looping over all threads). */
			list_add(&obj->list, &container->head);
			TLS_DATA_CONTAINER_UNLOCK(container);
		}
		else {
			TLS_DATA_CONTAINER_UNLOCK(container);

			/* Give the code above us a chance to cleanup. */
			if (container->free_function)
				container->free_function(obj);

			free(obj);
			obj = NULL;
			_stp_error("Couldn't setspecific on tls key: %d\n",
				   rc);
		}
	}
exit:
	return obj;
}

static void
_stp_tls_data_container_update(struct tls_data_container_t *container,
			       int (*init_function)(struct tls_data_object_t *),
			       void (*free_function)(struct tls_data_object_t *))
{
	container->init_function = init_function;
	container->free_function = free_function;
}

static int
_stp_tls_data_container_init(struct tls_data_container_t *container,
			     size_t size,
			     int (*init_function)(struct tls_data_object_t *),
			     void (*free_function)(struct tls_data_object_t *))
{
	int rc;

	INIT_LIST_HEAD(&container->head);
	if ((rc = TLS_DATA_CONTAINER_INIT(container)) != 0) {
	    _stp_error("Couldn't init tls lock: %d\n", rc);
	    return 1;
	}
	if ((rc = pthread_key_create(&container->key,
				     &_stp_tls_free_per_thread_ptr)) != 0) {
		_stp_error("Couldn't create tls key: %d\n", rc);
		TLS_DATA_CONTAINER_DESTROY(container);
		return 1;
	}

	container->size = size;
	container->init_function = init_function;
	container->free_function = free_function;
	return 0;
}

static void
_stp_tls_data_container_cleanup(struct tls_data_container_t *container)
{
	struct tls_data_object_t *obj, *n;

	TLS_DATA_CONTAINER_WRLOCK(container);
	(void) pthread_key_delete(container->key);
	for_each_tls_data_safe(obj, n, container) {
		list_del(&obj->list);

		/* Give the code above us a chance to cleanup. */
		if (container->free_function)
			container->free_function(obj);

		free(obj);
	}
	TLS_DATA_CONTAINER_UNLOCK(container);
	TLS_DATA_CONTAINER_DESTROY(container);
}
#endif /* _TLS_DATA_C_ */
