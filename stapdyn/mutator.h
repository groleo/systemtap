// stapdyn mutator functions
// Copyright (C) 2012-2013 Red Hat Inc.
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
extern "C" {
#include "../runtime/dyninst/stapdyn.h"
}


// The mutator drives all instrumentation.
class mutator {
  private:
    BPatch patch;

    void* module; // the locally dlopened probe module
    std::string module_name; // the filename of the probe module
    std::string module_shmem; // the global name of this module's shared memory
    std::vector<dynprobe_target> targets; // the probe targets in the module

    std::vector<boost::shared_ptr<mutatee> > mutatees; // all attached target processes
    boost::shared_ptr<mutatee> target_mutatee; // the main target process we created or attached
    bool p_target_created; // we only kill and wait on the target we created

    unsigned signal_count; // how many exit signals we've received

    // disable implicit constructors by not implementing these
    mutator (const mutator& other);
    mutator& operator= (const mutator& other);

    // Initialize the module session
    bool run_module_init();

    // Shutdown the module session
    bool run_module_exit();

    // Check the status of all mutatees
    bool update_mutatees();

    // Do probes matching 'flag' exist?
    bool matching_probes_exist(uint64_t flag);

    // Find a mutatee which matches the given process, else return NULL
    boost::shared_ptr<mutatee> find_mutatee(BPatch_process* process);

    // Stashed utrace probe enter function pointer.
    typeof(&enter_dyninst_utrace_probe) utrace_enter_fn;
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

    // Callback to respond to post fork events.  Check if it matches
    // our targets, and handle accordingly.
    void post_fork_callback(BPatch_thread *parent, BPatch_thread *child);
    void exec_callback(BPatch_thread *thread);
    void exit_callback(BPatch_thread *thread, BPatch_exitType type);

    void thread_create_callback(BPatch_process *proc, BPatch_thread *thread);
    void thread_destroy_callback(BPatch_process *proc, BPatch_thread *thread);

    // Callback to respond to signals.
    void signal_callback(int signal);
};


#endif // MUTATOR_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
