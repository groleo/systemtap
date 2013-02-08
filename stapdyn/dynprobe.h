// stapdyn probe declarations
// Copyright (C) 2012-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef DYNPROBE_H
#define DYNPROBE_H

#include <string>
#include <vector>

extern "C" {
#include <stdint.h>
}


// The individual probes' info read from the stap module.
struct dynprobe_location {
    uint64_t index;     // The index as counted by the module.
    uint64_t offset;    // The file offset of the probe's address.
    uint64_t semaphore; // The file offset of the probe's semaphore.
    uint64_t flags;	// The probe's flags.
    bool return_p;      // This is flagged as a return probe

    dynprobe_location(uint64_t index, uint64_t offset,
                      uint64_t semaphore, uint64_t flags);

    bool validate();
};


// The probe target info read from the stap module.
struct dynprobe_target {
    std::string path; // The fully resolved path to the file.
    std::vector<dynprobe_location> probes; // All probes in this target.
    dynprobe_target(const char* path): path(path) {}
};


// Look for probes in the stap module which need Dyninst instrumentation.
int find_dynprobes(void* module, std::vector<dynprobe_target>& targets);

#endif // DYNPROBE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
