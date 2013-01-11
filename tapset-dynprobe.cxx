// Synthetic derived probe group that enables merging probes from
// difference derived probe groups for dyninst.
//
// Copyright (C) 2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.


#include "session.h"
#include "tapsets.h"
#include "tapset-dynprobe.h"
#include "translate.h"

#include <cstring>
#include <string>

using namespace std;
using namespace __gnu_cxx;

// ------------------------------------------------------------------------
// dynprobe derived 'probes': These don't really exist. The purpose of
// the dynprobe_derived_probe_group is allow multiple
// *_derived_probe_group classes to output only one set of
// 'stapdu_target'/'stapdu_probe' arrays. This class also includes
// "dyninst/uprobes.c" only once and in the correct place.
// ------------------------------------------------------------------------

struct dynprobe_derived_probe: public derived_probe
{
  // Dummy constructor for gcc 3.4 compatibility
  dynprobe_derived_probe (): derived_probe (0, 0) { assert(0); }
};

struct dynprobe_info
{
  bool has_path;
  const Dwarf_Addr offset;
  const Dwarf_Addr semaphore_addr;
  const string flags_string;
  const string probe_init;

  dynprobe_info(bool hp, const Dwarf_Addr o, const Dwarf_Addr sa,
		const string fs, const string pi):
    has_path(hp), offset(o), semaphore_addr(sa), flags_string(fs),
    probe_init(pi) { }
};

struct dynprobe_derived_probe_group: public generic_dpg<dynprobe_derived_probe>
{
private:
  map<string, vector<dynprobe_info*> > info_by_path;
  typedef map<string, vector<dynprobe_info*> >::iterator i_b_path_iterator;
  map<Dwarf_Addr, vector<dynprobe_info*> > info_by_pid;
  typedef map<Dwarf_Addr, vector<dynprobe_info*> >::iterator i_b_pid_iterator;

  void emit_info(systemtap_session& s, unsigned tgt_idx, dynprobe_info *info);

public:
  void emit_module_decls (systemtap_session& s);
  void emit_module_init (systemtap_session& ) { }
  void emit_module_post_init (systemtap_session& ) { }
  void emit_module_exit (systemtap_session& ) { }

  void add(const string& path, const Dwarf_Addr offset,
	   const Dwarf_Addr semaphore_addr, const string& flags_string,
	   const string& probe_init);
};


void
dynprobe_derived_probe_group::add(const string& path, const Dwarf_Addr offset,
				  const Dwarf_Addr semaphore_addr,
				  const string& flags_string,
				  const string& probe_init)
{
  struct dynprobe_info *info = new dynprobe_info(!path.empty(), offset,
						 semaphore_addr,
						 flags_string, probe_init);
  if (!path.empty())
    info_by_path[path].push_back(info);
  else
    info_by_pid[offset].push_back(info);
}

void
dynprobe_derived_probe_group::emit_info(systemtap_session& s,
					unsigned tgt_idx, dynprobe_info *info)
{
  s.op->newline() << "{";
  s.op->line() << " .target=" << tgt_idx << ",";

  // We have to force path vs. pid probes into
  // stapdu_target/stapdu_probe. So, here's how we'll do it.
  //
  // For path probes, stapdu_target will hold the path, and the
  // stapdu_probe with the correct index will point to it.
  //
  // For pid probes, stapdu_target will have an empty path, and the
  // 'offset' field of stapdu_probe will really be the pid.
  if (info->has_path)
    s.op->line() << " .offset=" << lex_cast_hex(info->offset) << "ULL,";
  else
    s.op->line() << " .offset=" << info->offset << "ULL/*pid*/,";
  if (info->semaphore_addr)
    s.op->line() << " .semaphore=" << lex_cast_hex(info->semaphore_addr)
		 << "ULL,";
  s.op->line() << " .flags=" << info->flags_string << ",";
  s.op->line() << " .probe=" << info->probe_init << ",";
  s.op->line() << " },";
}

void
dynprobe_derived_probe_group::emit_module_decls (systemtap_session& s)
{
  if (! s.runtime_usermode_p())
    return;

  s.op->newline() << "#include \"dyninst/uprobes.h\"";
  s.op->newline() << "static const struct stapdu_target stapdu_targets[] = {";
  s.op->indent(1);
  for (i_b_path_iterator it = info_by_path.begin();
       it != info_by_path.end(); it++)
    {
      s.op->newline() << "{";
      s.op->line() << " .path=" << lex_cast_qstring(it->first) << ",";
      s.op->line() << " },";
    }
  if (! info_by_pid.empty())
    {
      s.op->newline() << "{";
      s.op->line() << " .path=NULL,";
      s.op->line() << " },";
    }
  s.op->newline(-1) << "};";
  s.op->assert_0_indent();

  s.op->newline() << "static struct stapdu_probe stapdu_probes[] = {";
  s.op->indent(1);
  unsigned tgt_idx = 0;
  for (i_b_path_iterator it = info_by_path.begin();
       it != info_by_path.end(); it++, tgt_idx++)
    {
      for (unsigned i = 0; i < it->second.size(); i++)
        {
	  dynprobe_info *info = it->second[i];
	  emit_info(s, tgt_idx, info);
	}
    }
  if (! info_by_pid.empty())
    {
      for (i_b_pid_iterator it = info_by_pid.begin();
	   it != info_by_pid.end(); it++)
        {
	  for (unsigned i = 0; i < it->second.size(); i++)
	    {
	      dynprobe_info *info = it->second[i];
	      emit_info(s, tgt_idx, info);
	    }
	}
    }
  s.op->newline(-1) << "};";
  s.op->assert_0_indent();

  s.op->newline() << "#include \"dyninst/uprobes.c\"";
}


// Declare that dynprobes are needed in this session
void enable_dynprobes(systemtap_session& s)
{
  if (! s.dynprobe_derived_probes)
    s.dynprobe_derived_probes = new dynprobe_derived_probe_group();
}

void
dynprobe_add_uprobe(systemtap_session& s, const string& path,
		    const Dwarf_Addr offset, const Dwarf_Addr semaphore_addr,
		    const string flags_string, const string probe_init)
{
  enable_dynprobes(s);
  s.dynprobe_derived_probes->add(path, offset, semaphore_addr, flags_string,
				 probe_init);
}
		    
void
dynprobe_add_utrace_path(systemtap_session& s, const std::string& path,
			 const std::string flags_string,
			 const std::string probe_init)
{
  enable_dynprobes(s);
  s.dynprobe_derived_probes->add(path, 0, 0, flags_string, probe_init);
}

void
dynprobe_add_utrace_pid(systemtap_session& s, const Dwarf_Addr pid,
			const std::string flags_string,
			const std::string probe_init)
{
  enable_dynprobes(s);
  // Notice we're passing the pid as the offset. Stapdyn is expecting
  // this when the path is empty.
  s.dynprobe_derived_probes->add("", pid, 0, flags_string, probe_init);
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
