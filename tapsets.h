// -*- C++ -*-
// Copyright (C) 2005-2010 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef TAPSETS_H
#define TAPSETS_H

#include "config.h"
#include "staptree.h"
#include "elaborate.h"

void check_process_probe_kernel_support(systemtap_session& s);

void register_standard_tapsets(systemtap_session& sess);
std::vector<derived_probe_group*> all_session_groups(systemtap_session& s);
std::string common_probe_init (derived_probe* p);
void common_probe_entryfn_prologue (systemtap_session& s, std::string statestr,
				    std::string probe, std::string probe_type,
				    bool overload_processing = true);
void common_probe_entryfn_epilogue (systemtap_session& s,
				    bool overload_processing);

void register_tapset_been(systemtap_session& sess);
void register_tapset_itrace(systemtap_session& sess);
void register_tapset_mark(systemtap_session& sess);
void register_tapset_procfs(systemtap_session& sess);
void register_tapset_timers(systemtap_session& sess);
void register_tapset_netfilter(systemtap_session& sess);
void register_tapset_perf(systemtap_session& sess);
void register_tapset_utrace(systemtap_session& sess);

std::string path_remove_sysroot(const systemtap_session& sess,
				const std::string& path);

// ------------------------------------------------------------------------
// Generic derived_probe_group: contains an ordinary vector of the
// given type.  It provides only the enrollment function.

template <class DP> struct generic_dpg: public derived_probe_group
{
protected:
  std::vector <DP*> probes;
public:
  generic_dpg () {}
  void enroll (DP* probe) { probes.push_back (probe); }
};


// ------------------------------------------------------------------------
// An update visitor that allows replacing assignments with a function call

struct var_expanding_visitor: public update_visitor
{
  static unsigned tick;
  std::stack<functioncall**> target_symbol_setter_functioncalls;
  std::stack<defined_op*> defined_ops;
  std::set<std::string> valid_ops;
  const std::string *op;

  var_expanding_visitor ();
  void visit_assignment (assignment* e);
  void visit_pre_crement (pre_crement* e);
  void visit_post_crement (post_crement* e);
  void visit_delete_statement (delete_statement* s);
  void visit_defined_op (defined_op* e);

private:
  bool rewrite_lvalue(const token *tok, const std::string& eop,
                      expression*& lvalue, expression*& rvalue);
};

#endif // TAPSETS_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
