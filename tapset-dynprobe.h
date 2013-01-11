// -*- C++ -*-
// Copyright (C) 2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef TAPSET_DYNPROBE_H
#define TAPSET_DYNPROBE_H

// Declare that dynprobes are needed in this session
void enable_dynprobes(systemtap_session& s);

void
dynprobe_add_uprobe(systemtap_session& s, const std::string& path,
		    const Dwarf_Addr offset, const Dwarf_Addr semaphore_addr,
		    const std::string flags_string,
		    const std::string probe_init);
		    
void
dynprobe_add_utrace_path(systemtap_session& s, const std::string& path,
			 const std::string flags_string,
			 const std::string probe_init);

void
dynprobe_add_utrace_pid(systemtap_session& s, const Dwarf_Addr pid,
			const std::string flags_string,
			const std::string probe_init);


void
dynprobe_add(systemtap_session& s, const std::string& path,
	     const Dwarf_Addr offset, const Dwarf_Addr semaphore_addr,
	     const std::string flags_string, const std::string probe_init);

#endif // TAPSET_DYNPROBE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
