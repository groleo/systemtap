// stapdyn mutatee declarations
// Copyright (C) 2012 Red Hat Inc.
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
    BPatch_process* process; // Dyninst's handle for this process
    BPatch_object* stap_dso; // the injected stap module

    std::vector<BPatch_snippet*> registers; // PC + DWARF registers

    // disable implicit constructors by not implementing these
    mutatee (const mutatee& other);
    mutatee& operator= (const mutatee& other);

  public:
    mutatee(BPatch_process* process);
    ~mutatee() {}

    bool operator==(BPatch_process* other) { return process == other; }

    // Inject the stap module into the target process
    bool load_stap_dso(const std::string& filename);

    // Given a target and the matching object, instrument all of the probes
    // with calls to the stap_dso's entry function.
    void instrument_dynprobe_target(BPatch_object* object,
                                    const dynprobe_target& target);

    // Look for all matches between this object and the targets
    // we want to probe, then do the instrumentation.
    void instrument_object_dynprobes(BPatch_object* object,
                                     const std::vector<dynprobe_target>& targets);

    // Look for probe matches in all objects.
    void instrument_dynprobes(const std::vector<dynprobe_target>& targets);

    // Look up a stap function by name and invoke it without parameters.
    void call_function(const std::string& name);

    void continue_execution() { process->continueExecution(); }
    bool is_terminated() { return process->isTerminated(); }
    bool check_exit() { return check_dyninst_exit(process); }
};

#endif // MUTATEE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
