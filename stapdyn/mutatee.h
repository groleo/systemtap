// stapdyn mutatee declarations
// Copyright (C) 2012-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef MUTATEE_H
#define MUTATEE_H

#include <string>
#include <vector>

#include <BPatch_object.h>
#include <BPatch_process.h>
#include <BPatch_snippet.h>

#include "dynprobe.h"
#include "dynutil.h"


// A mutatee is created for each attached process
class mutatee {
  private:
    pid_t pid; // The system's process ID.
    BPatch_process* process; // Dyninst's handle for this process
    BPatch_object* stap_dso; // the injected stap module

    std::vector<BPatch_snippet*> registers; // PC + DWARF registers

    std::vector<BPatchSnippetHandle*> snippets; // handles from insertSnippet

    std::vector<BPatch_variableExpr*> semaphores; // SDT semaphore variables

    std::vector<dynprobe_location> attached_probes;
    BPatch_function* utrace_enter_function;

    // disable implicit constructors by not implementing these
    mutatee (const mutatee& other);
    mutatee& operator= (const mutatee& other);

    void update_semaphores(unsigned short delta, size_t start=0);

    void call_utrace_dynprobe(const dynprobe_location& probe,
                              BPatch_thread* thread=NULL);
    void instrument_utrace_dynprobe(const dynprobe_location& probe);
    void instrument_global_dynprobe_target(const dynprobe_target& target);
    void instrument_global_dynprobes(const std::vector<dynprobe_target>& targets);

  public:
    mutatee(BPatch_process* process);
    ~mutatee();

    bool operator==(BPatch_process* other) { return process == other; }

    // Inject the stap module into the target process
    bool load_stap_dso(const std::string& filename);

    // Unload the stap module from the target process
    void unload_stap_dso();

    // Given a target and the matching object, instrument all of the probes
    // with calls to the stap_dso's entry function.
    void instrument_dynprobe_target(BPatch_object* object,
                                    const dynprobe_target& target);

    // Look for all matches between this object and the targets
    // we want to probe, then do the instrumentation.
    void instrument_object_dynprobes(BPatch_object* object,
                                     const std::vector<dynprobe_target>& targets);

    // Look for probe matches in all objects.
    void instrument_dynprobes(const std::vector<dynprobe_target>& targets,
                              bool after_exec_p=false);

    // Copy data for forked instrumentation
    void copy_forked_instrumentation(mutatee& other);

    // Reset instrumentation after an exec
    void exec_reset_instrumentation();

    // Remove all BPatch snippets we've instrumented in the target
    void remove_instrumentation();

    // Look up a stap function by name and invoke it, optionally with parameters.
    void call_function(const std::string& name);
    void call_function(const std::string& name,
                       const std::vector<BPatch_snippet *>& args);

    // Send a signal to the process.
    int kill(int signal);

    bool stop_execution();
    void continue_execution();
    void terminate_execution() { process->terminateExecution(); }

    bool is_stopped() { return process->isStopped(); }
    bool is_terminated() { return process->isTerminated(); }

    bool check_exit() { return check_dyninst_exit(process); }

    void begin_callback();
    void exit_callback(BPatch_thread *thread);
    void thread_callback(BPatch_thread *thread, bool create_p);

    void find_attached_probes(uint64_t flag,
			      std::vector<const dynprobe_location *>&probes);
};

#endif // MUTATEE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
