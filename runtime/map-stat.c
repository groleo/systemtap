/* -*- linux-c -*- 
 * map functions to handle statistics
 * Copyright (C) 2005, 2012 Red Hat Inc.
 *
 * This file is part of systemtap, and is free software.  You can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License (GPL); either version 2, or (at your option) any
 * later version.
 */

/** @file map-stat.c
 * @brief Map functions to handle statistics.
 */

#include "stat-common.c"

static void _stp_map_print_histogram (MAP map, stat_data *sd)
{
	_stp_stat_print_histogram (&map->hist, sd);
}

static MAP _stp_map_new_hstat_log (unsigned max_entries, int key_size)
{
	/* add size for buckets */
	int size = HIST_LOG_BUCKETS * sizeof(int64_t) + sizeof(stat_data);
	MAP m = _stp_map_new (max_entries, STAT, key_size, size);
	if (m) {
		m->hist.type = HIST_LOG;
		m->hist.buckets = HIST_LOG_BUCKETS;
	}
	return m;
}

static MAP _stp_map_new_hstat_linear (unsigned max_entries, int ksize, int start, int stop, int interval)
{
	MAP m;
	int size;
	int buckets = _stp_stat_calc_buckets(stop, start, interval);
	if (!buckets)
		return NULL;
	
        /* add size for buckets */
	size = buckets * sizeof(int64_t) + sizeof(stat_data);
	
	m = _stp_map_new (max_entries, STAT, ksize, size);
	if (m) {
		m->hist.type = HIST_LINEAR;
		m->hist.start = start;
		m->hist.stop = stop;
		m->hist.interval = interval;
		m->hist.buckets = buckets;
	}
	return m;
}

#ifndef __KERNEL__
static int _stp_map_tls_object_init(struct tls_data_object_t *obj);
static void _stp_map_tls_object_free(struct tls_data_object_t *obj);

static int _stp_hstat_tls_object_init(struct tls_data_object_t *obj)
{
	MAP m = container_of(obj, struct map_root, object);
	PMAP p = container_of(obj->container, struct pmap, container);

	if (_stp_map_tls_object_init(obj) != 0)
		return -1;

	/* Copy the hist params from the agg. */
	m->hist.type = p->agg.hist.type;
	m->hist.start = p->agg.hist.start;
	m->hist.stop = p->agg.hist.stop;
	m->hist.interval = p->agg.hist.interval;
	m->hist.buckets = p->agg.hist.buckets;
	return 0;
}
#endif

static PMAP _stp_pmap_new_hstat_linear (unsigned max_entries, int ksize, int start, int stop, int interval)
{
	PMAP pmap;
	int size;
	int buckets = _stp_stat_calc_buckets(stop, start, interval);
	if (!buckets)
		return NULL;

        /* add size for buckets */
	size = buckets * sizeof(int64_t) + sizeof(stat_data);

	pmap = _stp_pmap_new (max_entries, STAT, ksize, size);
	if (pmap) {
		int i;
		MAP m;
#ifdef __KERNEL__
		for_each_possible_cpu(i) {
			m = (MAP)per_cpu_ptr (pmap->map, i);
			m->hist.type = HIST_LINEAR;
			m->hist.start = start;
			m->hist.stop = stop;
			m->hist.interval = interval;
			m->hist.buckets = buckets;
		}
#else
		/* Override the tls data object init function with one
		 * that knows how to handle hstats. */
		_stp_tls_data_container_update(&pmap->container,
					       &_stp_hstat_tls_object_init,
					       &_stp_map_tls_object_free);
#endif
		/* now set agg map params */
		m = &pmap->agg;
		m->hist.type = HIST_LINEAR;
		m->hist.start = start;
		m->hist.stop = stop;
		m->hist.interval = interval;
		m->hist.buckets = buckets;
	}
	return pmap;
}

static PMAP _stp_pmap_new_hstat_log (unsigned max_entries, int key_size)
{
	/* add size for buckets */
	int size = HIST_LOG_BUCKETS * sizeof(int64_t) + sizeof(stat_data);
	PMAP pmap = _stp_pmap_new (max_entries, STAT, key_size, size);
	if (pmap) {
		int i;
		MAP m;
#ifdef __KERNEL__
		for_each_possible_cpu(i) {
			m = (MAP)per_cpu_ptr (pmap->map, i);
			m->hist.type = HIST_LOG;
			m->hist.buckets = HIST_LOG_BUCKETS;
		}
#else
		/* Override the tls data object init function with one
		 * that knows how to handle hstats. */
		_stp_tls_data_container_update(&pmap->container,
					       &_stp_hstat_tls_object_init,
					       &_stp_map_tls_object_free);
#endif
		/* now set agg map  params */
		m = &pmap->agg;
		m->hist.type = HIST_LOG;
		m->hist.buckets = HIST_LOG_BUCKETS;
	}
	return pmap;
}
