// stapdyn probe functions
// Copyright (C) 2012-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "dynprobe.h"
#include "dynutil.h"
#include "../util.h"

#include "../runtime/dyninst/stapdyn.h"


using namespace std;


// Look for probes in the stap module which need Dyninst instrumentation.
int
find_dynprobes(void* module, vector<dynprobe_target>& targets)
{
  // We query for probes with a function interface, so first we have
  // to get function pointers from the stap module.
  __typeof__(&stp_dyninst_target_count) target_count = NULL;
  __typeof__(&stp_dyninst_target_path) target_path = NULL;

  __typeof__(&stp_dyninst_probe_count) probe_count = NULL;
  __typeof__(&stp_dyninst_probe_target) probe_target = NULL;
  __typeof__(&stp_dyninst_probe_offset) probe_offset = NULL;
  __typeof__(&stp_dyninst_probe_semaphore) probe_semaphore = NULL;

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

  // This is optional - was only added post-2.0
  __typeof__(&stp_dyninst_probe_flags) probe_flags = NULL;
  set_dlsym(probe_flags, module, "stp_dyninst_probe_flags", false);

  // Construct all the targets in the module.
  const uint64_t ntargets = target_count();
  for (uint64_t i = 0; i < ntargets; ++i)
    {
      const char* path = target_path(i);
      if (path == NULL)
	  path = "";
      dynprobe_target target(path);
      targets.push_back(target);
    }

  // Construct all the probes in the module,
  // and associate each with their target.
  const uint64_t nprobes = probe_count();
  for (uint64_t i = 0; i < nprobes; ++i)
    {
      uint64_t target_index = probe_target(i);
      uint64_t offset = probe_offset(i);
      uint64_t semaphore = probe_semaphore(i);
      uint64_t flags = probe_flags ? probe_flags(i) : 0;
      dynprobe_location p(i, offset, semaphore, flags);
      if (p.validate() && target_index < ntargets)
        targets[target_index].probes.push_back(p);
    }

  // For debugging, dump what we found.
  for (uint64_t i = 0; i < ntargets; ++i)
    {
      dynprobe_target& t = targets[i];
      staplog(3) << "target \"" << t.path << "\" has "
                 << t.probes.size() << " probes" << endl;
      for (uint64_t j = 0; j < t.probes.size(); ++j)
        staplog(3) << "  offset:" << lex_cast_hex(t.probes[j].offset)
                   << " semaphore:" << lex_cast_hex(t.probes[j].semaphore)
                   << " flags:" << t.probes[j].flags << endl;
    }

  return 0;
}


dynprobe_location::dynprobe_location(uint64_t index, uint64_t offset,
                                     uint64_t semaphore, uint64_t flags):
      index(index), offset(offset), semaphore(semaphore),
      flags(flags), return_p(flags & STAPDYN_PROBE_FLAG_RETURN)
{
}

bool
dynprobe_location::validate()
{
  if (flags & ~STAPDYN_PROBE_ALL_FLAGS)
    {
      stapwarn() << "Unknown flags " << lex_cast_hex(flags)
                 << " in probe " << index << endl;
      return false;
    }

  return true;
}


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
