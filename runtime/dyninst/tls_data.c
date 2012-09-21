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
	pthread_mutex_t lock;		/* lock protecting list */
	int (*init_function)(struct tls_data_object_t *);
	void (*free_function)(struct tls_data_object_t *);
};

struct tls_data_object_t {
	struct list_head list;
	struct tls_data_container_t *container;
};

#define TLS_DATA_CONTAINER_LOCK(con) pthread_mutex_lock(&(con)->lock)
#define TLS_DATA_CONTAINER_UNLOCK(con) pthread_mutex_unlock(&(con)->lock)

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
			pthread_mutex_lock(&container->lock);
			list_del(&obj->list);

			/* Give the code above us a chance to cleanup. */
			if (container->free_function)
				container->free_function(obj);
		}

		/* Note that this free() call only works correctly if
		 * the struct tls_data_object is the first thing in
		 * its containing structure. */
		free(obj);

		if (container)
			pthread_mutex_unlock(&container->lock);
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
		pthread_mutex_lock(&container->lock);
		if ((rc = pthread_setspecific(container->key, obj)) == 0) {
			/* Add obj to container's list of objs (for
			 * use in looping over all threads). */
			list_add(&obj->list, &container->head);
		}
		else {
			_stp_error("Couldn't setspecific on tls key: %d\n",
				   rc);

			/* Give the code above us a chance to cleanup. */
			if (container->free_function)
				container->free_function(obj);

			free(obj);
			obj = NULL;
		}
		pthread_mutex_unlock(&container->lock);
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
	if ((rc = pthread_mutex_init(&container->lock, NULL)) != 0) {
	    _stp_error("Couldn't init tls mutex: %d\n", rc);
	    return 1;
	}
	if ((rc = pthread_key_create(&container->key,
				     &_stp_tls_free_per_thread_ptr)) != 0) {
		_stp_error("Couldn't create tls key: %d\n", rc);
		(void)pthread_mutex_destroy(&container->lock);
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

	TLS_DATA_CONTAINER_LOCK(container);
	(void) pthread_key_delete(container->key);
	for_each_tls_data_safe(obj, n, container) {
		list_del(&obj->list);

		/* Give the code above us a chance to cleanup. */
		if (container->free_function)
			container->free_function(obj);

		free(obj);
	}
	TLS_DATA_CONTAINER_UNLOCK(container);
	(void)pthread_mutex_destroy(&container->lock);
}
#endif /* _TLS_DATA_C_ */
