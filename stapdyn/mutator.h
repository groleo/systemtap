// stapdyn mutator functions
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef MUTATOR_H
#define MUTATOR_H

#include <string>
#include <vector>

#include <boost/shared_ptr.hpp>

#include <BPatch.h>
#include <BPatch_module.h>
#include <BPatch_process.h>
#include <BPatch_thread.h>

#include "dynprobe.h"
#include "dynutil.h"
#include "mutatee.h"


// The mutator drives all instrumentation.
class mutator {
  private:
    BPatch patch;

    void* module; // the locally dlopened probe module
    std::string module_name; // the filename of the probe module
    std::vector<dynprobe_target> targets; // the probe targets in the module

    std::vector<boost::shared_ptr<mutatee> > mutatees; // all attached target processes

    // disable implicit constructors by not implementing these
    mutator (const mutator& other);
    mutator& operator= (const mutator& other);

    // When there's no target command given,
    // just run the begin/end basics directly.
    bool run_simple();

  public:

    mutator (const std::string& module_name);
    ~mutator ();

    // Load the stap module and initialize all probe info.
    bool load ();

    // Create a new process with the given command line.
    bool create_process(const std::string& command);

    // Attach to a specific existing process.
    bool attach_process(BPatch_process* process);
    bool attach_process(pid_t pid);

    // Start the actual systemtap session!
    bool run ();

    // Callback to respond to dynamically loaded libraries.
    // Check if it matches our targets, and instrument accordingly.
    void dynamic_library_callback(BPatch_thread *thread,
                                  BPatch_module *module,
                                  bool load);
};


#endif // MUTATOR_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
