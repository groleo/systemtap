// stapdyn probe functions
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "dynprobe.h"
#include "dynutil.h"

#include "../runtime/dyninst/stapdyn.h"


using namespace std;


// Look for probes in the stap module which need Dyninst instrumentation.
int
find_dynprobes(void* module, vector<dynprobe_target>& targets)
{
  // We query for probes with a function interface, so first we have
  // to get function pointers from the stap module.
  typeof(&stp_dyninst_target_count) target_count = NULL;
  typeof(&stp_dyninst_target_path) target_path = NULL;

  typeof(&stp_dyninst_probe_count) probe_count = NULL;
  typeof(&stp_dyninst_probe_target) probe_target = NULL;
  typeof(&stp_dyninst_probe_offset) probe_offset = NULL;
  typeof(&stp_dyninst_probe_semaphore) probe_semaphore = NULL;

  // If we don't even have this, then there aren't any uprobes in the module.
  set_dlsym(target_count, module, "stp_dyninst_target_count", false);
  if (target_count == NULL)
    return 0;

  try
    {
      // If target_count exists, the rest of these should too.
      set_dlsym(target_path, module, "stp_dyninst_target_path");
      set_dlsym(probe_count, module, "stp_dyninst_probe_count");
      set_dlsym(probe_target, module, "stp_dyninst_probe_target");
      set_dlsym(probe_offset, module, "stp_dyninst_probe_offset");
      set_dlsym(probe_semaphore, module, "stp_dyninst_probe_semaphore");
    }
  catch (runtime_error& e)
    {
      staperror() << e.what() << endl;
      return 1;
    }

  // Construct all the targets in the module.
  const uint64_t ntargets = target_count();
  for (uint64_t i = 0; i < ntargets; ++i)
    targets.push_back(dynprobe_target(target_path(i)));

  // Construct all the probes in the module,
  // and associate each with their target.
  const uint64_t nprobes = probe_count();
  for (uint64_t i = 0; i < nprobes; ++i)
    {
      uint64_t ti = probe_target(i);
      dynprobe_location p(i, probe_offset(i), probe_semaphore(i));
      if (ti < ntargets)
        targets[ti].probes.push_back(p);
    }

  // For debugging, dump what we found.
  for (uint64_t i = 0; i < ntargets; ++i)
    {
      dynprobe_target& t = targets[i];
      staplog(3) << "target " << t.path << " has "
                 << t.probes.size() << " probes" << endl;
      for (uint64_t j = 0; j < t.probes.size(); ++j)
        staplog(3) << "  offset:" << (void*)t.probes[j].offset
                   << " semaphore:" << t.probes[j].semaphore << endl;
    }

  return 0;
}


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
