// stapdyn utility functions
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef DYNUTIL_H
#define DYNUTIL_H

#include <ostream>
#include <stdexcept>
#include <string>

#include <BPatch_process.h>

// Check that environment DYNINSTAPI_RT_LIB exists and is a valid file.
// If not, try to guess a good value and set it.
bool check_dyninst_rt(void);

// Check that SELinux settings are ok for Dyninst operation.
bool check_dyninst_sebools(void);

// Check whether a process exited cleanly
bool check_dyninst_exit(BPatch_process *process);


// Get an untyped symbol from a dlopened module.
// If flagged as 'required', throw an exception if missing or NULL.
void* get_dlsym(void* handle, const char* symbol, bool required=true);

// Set a typed pointer by looking it up in a dlopened module.
// If flagged as 'required', throw an exception if missing or NULL.
template <typename T> void
set_dlsym(T*& pointer, void* handle, const char* symbol, bool required=true)
{
  pointer = reinterpret_cast<T*>(get_dlsym(handle, symbol, required));
}


//
// Logging, warnings, and errors, oh my!
//

// verbosity, increased by -v
extern unsigned stapdyn_log_level;

// Whether to suppress warnings, set by -w
extern bool stapdyn_supress_warnings;

// Return a stream for logging at the given verbosity level.
std::ostream& staplog(unsigned level=0);

// Return a stream for warning messages.
std::ostream& stapwarn(void);

// Return a stream for error messages.
std::ostream& staperror(void);


#endif // DYNUTIL_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
