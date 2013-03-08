// -*- C++ -*-
// Copyright (C) 2005, 2009 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef TRANSLATE_H
#define TRANSLATE_H

#include "staptree.h"
#include "parse.h"
#include <iostream>
#include <fstream>
#include <string>


// ------------------------------------------------------------------------

// Output context for systemtap translation, intended to allow
// pretty-printing.
class translator_output
{
  char *buf;
  std::ofstream* o2;
  std::ostream& o;
  unsigned tablevel;

public:
  std::string filename;

  translator_output (std::ostream& file);
  translator_output (const std::string& filename, size_t bufsize = 8192);
  ~translator_output ();

  std::ostream& newline (int indent = 0);
  void indent (int indent = 0);
  void assert_0_indent () { o << std::flush; assert (tablevel == 0); }
  std::ostream& line();

  std::ostream::pos_type tellp() { return o.tellp(); }
  std::ostream& seekp(std::ostream::pos_type p) { return o.seekp(p); }
};


// An unparser instance is in charge of emitting code for generic
// probe bodies, functions, globals.
struct unparser
{
  virtual ~unparser () {}

  virtual void emit_common_header () = 0;
  // #include<...>
  //
  // #define MAXNESTING nnn
  // #define MAXCONCURRENCY mmm
  // #define MAXSTRINGLEN ooo
  //
  // enum session_state_t {
  //   starting, begin, running, suspended, errored, ending, ended
  // };
  // static atomic_t session_state;
  //
  // struct context {
  //   unsigned errorcount;
  //   unsigned busy;
  //   ...
  // } context [MAXCONCURRENCY];

  // struct {
  virtual void emit_global (vardecl* v) = 0;
  // TYPE s_NAME;  // NAME is prefixed with "s_" to avoid kernel id collisions
  // rwlock_t s_NAME_lock;
  // } global = {
  virtual void emit_global_init (vardecl* v) = 0;
  // };

  virtual void emit_global_param (vardecl* v) = 0;
  // module_param_... -- at end of file

  virtual void emit_functionsig (functiondecl* v) = 0;
  // static void function_NAME (context* c);

  virtual void emit_module_init () = 0;
  virtual void emit_module_refresh () = 0;
  virtual void emit_module_exit () = 0;
  // startup, probe refresh/activation, shutdown

  virtual void emit_function (functiondecl* v) = 0;
  // void function_NAME (struct context* c) {
  //   ....
  // }

  virtual void emit_probe (derived_probe* v) = 0;
  // void probe_NUMBER (struct context* c) {
  //   ... lifecycle
  //   ....
  // }
  // ... then call over to the derived_probe's emit_probe_entries() fn

  // The following helper functions must be used by any code-generation
  // infrastructure outside this file to properly mangle identifiers
  // appearing in the final code:
  virtual std::string c_localname (const std::string& e, bool mangle_oldstyle = false) = 0;
  virtual std::string c_globalname (const std::string &e) = 0;
  virtual std::string c_funcname (const std::string &e) = 0;
};


// Preparation done before checking the script cache, especially
// anything that might affect the hashed name.
int prepare_translate_pass (systemtap_session& s);

int translate_pass (systemtap_session& s);

#endif // TRANSLATE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
