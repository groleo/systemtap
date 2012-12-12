// -*- C++ -*-
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
// ... TODOXXX additional blurb for re2c ...

#ifndef REGCOMP_H
#define REGCOMP_H

#include <string>
#include <iostream>
#include <stdexcept>

// TODOXXX support for REGCOMP_STANDALONE
struct systemtap_session; /* from session.h */
struct translator_output; /* from translate.h */
namespace re2c {
  typedef unsigned int uint;
  class RegExp; /* from re2c */
  class DFA; /* from re2c */
};

struct stapdfa {
  std::string orig_input;
  stapdfa (); // TODOXXX get rid of need for this
  stapdfa (const std::string& func_name, const std::string& re);
  ~stapdfa ();
  void emit_declaration (translator_output *o);
  void emit_matchop (translator_output *o, const std::string& match_expr);
  void print (std::ostream& o) const;
private:
  re2c::RegExp *prepare_rule(re2c::RegExp *raw);
  std::string func_name;
  re2c::RegExp *ast;
  re2c::DFA *content;
  static re2c::RegExp *failRE; // hacky thing to attach {return 0;} to
  static re2c::RegExp *padRE; // hacky thing to pad the output on the left
  // TODOXXX I hope RegExp instances are really reusable!
};

std::ostream& operator << (std::ostream &o, const stapdfa& d);

struct dfa_parse_error: public std::runtime_error
{
  const std::string orig_input;
  unsigned pos;
  dfa_parse_error (const std::string& msg, const std::string& orig_input):
    runtime_error(msg), orig_input(orig_input), pos(0) {}
  dfa_parse_error (const std::string& msg, const std::string& orig_input,
                   unsigned pos):
    runtime_error(msg), orig_input(orig_input), pos(pos) {}
  ~dfa_parse_error () throw () {}
};

/* Retrieves the corresponding dfa from s->dfas if it is already created: */
stapdfa *regex_to_stapdfa (systemtap_session *s, const std::string& input);

#endif

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
