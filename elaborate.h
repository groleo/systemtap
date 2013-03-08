// -*- C++ -*-
// Copyright (C) 2005-2011 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#ifndef ELABORATE_H
#define ELABORATE_H

#include "staptree.h"
#include "parse.h"
#include <string>
#include <vector>
//#include <iostream>
#include <iosfwd>
#include <sstream>
#include <map>
#include <list>

extern "C" {
#include <elfutils/libdw.h>
}

#include "privilege.h"

struct recursive_expansion_error : public semantic_error
{
  ~recursive_expansion_error () throw () {}
  recursive_expansion_error (const std::string& msg, const token* t1=0):
    semantic_error (msg, t1) {}

  recursive_expansion_error (const std::string& msg, const token* t1,
                             const token* t2):
    semantic_error (msg, t1, t2) {}
};

// ------------------------------------------------------------------------

struct derived_probe;
class match_node;

struct symresolution_info: public traversing_visitor
{
protected:
  systemtap_session& session;

public:
  functiondecl* current_function;
  derived_probe* current_probe;
  symresolution_info (systemtap_session& s);

  vardecl* find_var (const std::string& name, int arity, const token *tok);
  functiondecl* find_function (const std::string& name, unsigned arity);

  void visit_block (block *s);
  void visit_symbol (symbol* e);
  void visit_foreach_loop (foreach_loop* e);
  void visit_arrayindex (arrayindex* e);
  void visit_functioncall (functioncall* e);
  void visit_delete_statement (delete_statement* s);
};


struct typeresolution_info: public visitor
{
  typeresolution_info (systemtap_session& s);
  systemtap_session& session;
  unsigned num_newly_resolved;
  unsigned num_still_unresolved;
  bool assert_resolvability;
  functiondecl* current_function;
  derived_probe* current_probe;
  std::vector <const token*> resolved_toks; // account for type mis-
  std::vector <const token*> printed_toks;  // matches (BZ 9719)

  void check_arg_type (exp_type wanted, expression* arg);
  void check_local (vardecl* v);
  void mismatch (const token* tok, exp_type t1, exp_type t2);
  void unresolved (const token* tok);
  void resolved (const token* tok, exp_type t);
  void invalid (const token* tok, exp_type t);

  exp_type t; // implicit parameter for nested visit call; may clobber
              // Upon entry to one of the visit_* calls, the incoming
              // `t' value is the type inferred for that node from 
              // context.  It may match or conflict with the node's 
              // preexisting type, or it may be unknown.

  void visit_block (block* s);
  void visit_try_block (try_block* s);
  void visit_embeddedcode (embeddedcode* s);
  void visit_null_statement (null_statement* s);
  void visit_expr_statement (expr_statement* s);
  void visit_if_statement (if_statement* s);
  void visit_for_loop (for_loop* s);
  void visit_foreach_loop (foreach_loop* s);
  void visit_return_statement (return_statement* s);
  void visit_delete_statement (delete_statement* s);
  void visit_next_statement (next_statement* s);
  void visit_break_statement (break_statement* s);
  void visit_continue_statement (continue_statement* s);
  void visit_literal_string (literal_string* e);
  void visit_literal_number (literal_number* e);
  void visit_embedded_expr (embedded_expr* e);
  void visit_binary_expression (binary_expression* e);
  void visit_unary_expression (unary_expression* e);
  void visit_pre_crement (pre_crement* e);
  void visit_post_crement (post_crement* e);
  void visit_logical_or_expr (logical_or_expr* e);
  void visit_logical_and_expr (logical_and_expr* e);
  void visit_array_in (array_in* e);
  void visit_regex_query (regex_query* e);
  void visit_comparison (comparison* e);
  void visit_concatenation (concatenation* e);
  void visit_ternary_expression (ternary_expression* e);
  void visit_assignment (assignment* e);
  void visit_symbol (symbol* e);
  void visit_target_symbol (target_symbol* e);
  void visit_arrayindex (arrayindex* e);
  void visit_functioncall (functioncall* e);
  void visit_print_format (print_format* e);
  void visit_stat_op (stat_op* e);
  void visit_hist_op (hist_op* e);
  void visit_cast_op (cast_op* e);
  void visit_defined_op (defined_op* e);
  void visit_entry_op (entry_op* e);
  void visit_perf_op (perf_op* e);
};


// ------------------------------------------------------------------------


// A derived_probe is a probe that has been elaborated by
// binding to a matching provider.  The locations std::vector
// may be smaller or larger than the base probe, since a
// provider may transform it.

class translator_output;
struct derived_probe_group;

struct derived_probe: public probe
{
  derived_probe (probe* b, probe_point* l, bool rewrite_loc=false);
  probe* base; // the original parsed probe
  probe_point* base_pp; // the probe_point that led to this derivation
  virtual const probe* basest () const { return base->basest(); }
  virtual const probe* almost_basest () const { return base->almost_basest() ?: this; }
  virtual ~derived_probe () {}
  virtual void join_group (systemtap_session& s) = 0;
  virtual probe_point* sole_location () const;
  virtual probe_point* script_location () const;
  virtual void printsig (std::ostream &o) const;
  // return arguments of probe if there
  virtual void getargs (std::list<std::string> &) const {}
  void printsig_nested (std::ostream &o) const;
  virtual void collect_derivation_chain (std::vector<probe*> &probes_list);
  virtual void collect_derivation_pp_chain (std::vector<probe_point*> &pp_list);
  std::string derived_locations ();

  virtual void print_dupe_stamp(std::ostream&) {}
  // To aid duplication elimination, print a stamp which uniquely identifies
  // the code that will be added to the probe body.  (Doesn't need to be the
  // actual code...)

  virtual void initialize_probe_context_vars (translator_output*) {}
  // From within unparser::emit_probe, initialized any extra variables
  // in this probe's context locals.

  virtual void emit_probe_local_init (systemtap_session& s, translator_output*) {}
  // From within unparser::emit_probe, emit any extra processing block
  // for this probe.

  virtual void emit_privilege_assertion (translator_output*);
  // From within unparser::emit_probe, emit any unprivileged mode
  // checking for this probe.

public:
  static void emit_common_header (translator_output* o);
  // from c_unparser::emit_common_header
  // XXX: probably can move this stuff to a probe_group::emit_module_decls

  static void emit_process_owner_assertion (translator_output*);
  // From within unparser::emit_probe, emit a check that the current
  // process belongs to the user.

  static void print_dupe_stamp_unprivileged(std::ostream& o);
  static void print_dupe_stamp_unprivileged_process_owner(std::ostream& o);

  virtual bool needs_global_locks () { return true; }
  // by default, probes need locks around global variables

  // Location of semaphores to activate sdt probes
  Dwarf_Addr sdt_semaphore_addr;

  // perf.counter probes that this probe references
  std::set<derived_probe*> perf_counter_refs;

  // index into session.probes[], set and used during translation
  unsigned session_index;
};

// ------------------------------------------------------------------------

struct unparser;

// Various derived classes derived_probe_group manage the
// registration/invocation/unregistration of sibling probes.
struct derived_probe_group
{
  virtual ~derived_probe_group () {}

  virtual void emit_module_decls (systemtap_session& s) = 0;
  // The _decls-generated code may assume that declarations such as
  // the context, embedded-C code, function and probe handler bodies
  // are all already generated.  That is, _decls is called near the
  // end of the code generation process.  It should minimize the
  // number of separate variables (and to a lesser extent, their
  // size).

  virtual void emit_module_init (systemtap_session& s) = 0;
  // The _init-generated code may assume that it is called only once.
  // If that code fails at run time, it must set rc=1 and roll back
  // any partial initializations, for its _exit friend will NOT be
  // invoked.  The generated code may use pre-declared "int i, j;"
  // and set "const char* probe_point;".

  virtual void emit_module_post_init (systemtap_session& s) {}
  // The emit_module_post_init() code is called once session_state is
  // set to running.

  virtual void emit_module_refresh (systemtap_session& s) {}
  // The _refresh-generated code may be called multiple times during
  // a session run, bracketed by _init and _exit calls.
  // Upon failure, it must set enough state so that
  // a subsequent _exit call will clean up everything.
  // The generated code may use pre-declared "int i, j;".

  virtual void emit_module_exit (systemtap_session& s) = 0;
  // The _exit-generated code may assume that it is executed exactly
  // zero times (if the _init-generated code failed) or once.  (_exit
  // itself may be called a few times, to generate the code in a few
  // different places in the probe module.)
  // The generated code may use pre-declared "int i, j;".
};


// ------------------------------------------------------------------------

typedef std::map<std::string, literal*> literal_map_t;

struct derived_probe_builder
{
  virtual void build(systemtap_session & sess,
		     probe* base,
		     probe_point* location,
		     literal_map_t const & parameters,
		     std::vector<derived_probe*> & finished_results) = 0;
  virtual void build_with_suffix(systemtap_session & sess,
                                 probe * use,
                                 probe_point * location,
                                 std::map<std::string, literal *>
                                   const & parameters,
                                 std::vector<derived_probe *>
                                   & finished_results,
                                 std::vector<probe_point::component *>
                                   const & suffix);
  virtual ~derived_probe_builder() {}
  virtual void build_no_more (systemtap_session &) {}
  virtual bool is_alias () const { return false; }

  static bool has_null_param (literal_map_t const & parameters,
                              const std::string& key);
  static bool get_param (literal_map_t const & parameters,
                         const std::string& key, std::string& value);
  static bool get_param (literal_map_t const & parameters,
                         const std::string& key, int64_t& value);
};


struct
match_key
{
  std::string name;
  bool have_parameter;
  exp_type parameter_type;

  match_key(std::string const & n);
  match_key(probe_point::component const & c);

  match_key & with_number();
  match_key & with_string();
  std::string str() const;
  bool operator<(match_key const & other) const;
  bool globmatch(match_key const & other) const;
};


class
match_node
{
  typedef std::map<match_key, match_node*> sub_map_t;
  typedef std::map<match_key, match_node*>::iterator sub_map_iterator_t;
  sub_map_t sub;
  std::vector<derived_probe_builder*> ends;

 public:
  match_node();

  void find_and_build (systemtap_session& s,
                       probe* p, probe_point *loc, unsigned pos,
                       std::vector<derived_probe *>& results);
  void try_suffix_expansion (systemtap_session& s,
                             probe *p, probe_point *loc, unsigned pos,
                             std::vector<derived_probe *>& results);
  void build_no_more (systemtap_session &s);
  void dump (systemtap_session &s, const std::string &name = "");

  match_node* bind(match_key const & k);
  match_node* bind(std::string const & k);
  match_node* bind_str(std::string const & k);
  match_node* bind_num(std::string const & k);
  match_node* bind_privilege(privilege_t p = privilege_t (pr_stapdev | pr_stapsys));
  void bind(derived_probe_builder* e);

private:
  privilege_t privilege;
};

// ------------------------------------------------------------------------

struct
alias_expansion_builder
  : public derived_probe_builder
{
  probe_alias * alias;

  alias_expansion_builder(probe_alias * a)
    : alias(a)
  {}

  virtual void build(systemtap_session & sess,
		     probe * use,
		     probe_point * location,
		     std::map<std::string, literal *> const &,
		     std::vector<derived_probe *> & finished_results);
  virtual void build_with_suffix(systemtap_session & sess,
                                 probe * use,
                                 probe_point * location,
                                 std::map<std::string, literal *>
                                   const &,
                                 std::vector<derived_probe *>
                                   & finished_results,
                                 std::vector<probe_point::component *>
                                   const & suffix);
  virtual bool is_alias () const { return true; }

  bool checkForRecursiveExpansion (probe *use);
};

// ------------------------------------------------------------------------

/* struct systemtap_session moved to session.h */

int semantic_pass (systemtap_session& s);
void derive_probes (systemtap_session& s,
                    probe *p, std::vector<derived_probe*>& dps,
                    bool optional = false, bool rethrow_errors = false);

// A helper we use here and in translate, for pulling symbols out of lvalue
// expressions.
symbol * get_symbol_within_expression (expression *e);


struct unparser;


#endif // ELABORATE_H

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
