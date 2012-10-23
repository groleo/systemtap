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

static MAP _stp_map_new_hstat_log (unsigned max_entries, int wrap, int key_size)
{
	/* add size for buckets */
	int size = HIST_LOG_BUCKETS * sizeof(int64_t) + sizeof(stat_data);
	MAP m = _stp_map_new (max_entries, wrap, STAT, key_size, size);
	if (m) {
		m->hist.type = HIST_LOG;
		m->hist.buckets = HIST_LOG_BUCKETS;
	}
	return m;
}

static MAP
_stp_map_new_hstat_linear (unsigned max_entries, int wrap, int ksize,
			   int start, int stop, int interval)
{
	MAP m;
	int size;
	int buckets = _stp_stat_calc_buckets(stop, start, interval);
	if (!buckets)
		return NULL;
	
        /* add size for buckets */
	size = buckets * sizeof(int64_t) + sizeof(stat_data);
	
	m = _stp_map_new (max_entries, wrap, STAT, ksize, size);
	if (m) {
		m->hist.type = HIST_LINEAR;
		m->hist.start = start;
		m->hist.stop = stop;
		m->hist.interval = interval;
		m->hist.buckets = buckets;
	}
	return m;
}


static PMAP
_stp_pmap_new_hstat_linear (unsigned max_entries, int wrap, int ksize,
			    int start, int stop, int interval)
{
	PMAP pmap;
	int size;
	int buckets = _stp_stat_calc_buckets(stop, start, interval);
	if (!buckets)
		return NULL;

        /* add size for buckets */
	size = buckets * sizeof(int64_t) + sizeof(stat_data);

	pmap = _stp_pmap_new (max_entries, wrap, STAT, ksize, size);
	if (pmap) {
		int i;
		MAP m;

		_stp_map_for_each_cpu(i) {
			m = (MAP)_stp_map_per_cpu_ptr (pmap->map, i);
			MAP_LOCK(m);
			m->hist.type = HIST_LINEAR;
			m->hist.start = start;
			m->hist.stop = stop;
			m->hist.interval = interval;
			m->hist.buckets = buckets;
			MAP_UNLOCK(m);
		}
		/* now set agg map params */
		m = &pmap->agg;
		MAP_LOCK(m);
		m->hist.type = HIST_LINEAR;
		m->hist.start = start;
		m->hist.stop = stop;
		m->hist.interval = interval;
		m->hist.buckets = buckets;
		MAP_UNLOCK(m);
	}
	return pmap;
}

static PMAP
_stp_pmap_new_hstat_log (unsigned max_entries, int wrap, int key_size)
{
	/* add size for buckets */
	int size = HIST_LOG_BUCKETS * sizeof(int64_t) + sizeof(stat_data);
	PMAP pmap = _stp_pmap_new (max_entries, wrap, STAT, key_size, size);
	if (pmap) {
		int i;
		MAP m;
		_stp_map_for_each_cpu(i) {
			m = (MAP)_stp_map_per_cpu_ptr (pmap->map, i);
			MAP_LOCK(m);
			m->hist.type = HIST_LOG;
			m->hist.buckets = HIST_LOG_BUCKETS;
			MAP_UNLOCK(m);
		}
		/* now set agg map params */
		m = &pmap->agg;
		MAP_LOCK(m);
		m->hist.type = HIST_LOG;
		m->hist.buckets = HIST_LOG_BUCKETS;
		MAP_UNLOCK(m);
	}
	return pmap;
}
