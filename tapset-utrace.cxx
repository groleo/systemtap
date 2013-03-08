// utrace tapset
// Copyright (C) 2005-2013 Red Hat Inc.
// Copyright (C) 2005-2007 Intel Corporation.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.


#include "session.h"
#include "tapsets.h"
#include "task_finder.h"
#include "tapset-dynprobe.h"
#include "translate.h"
#include "util.h"

#include <cstring>
#include <string>


using namespace std;
using namespace __gnu_cxx;


static const string TOK_PROCESS("process");
static const string TOK_BEGIN("begin");
static const string TOK_END("end");
static const string TOK_THREAD("thread");
static const string TOK_SYSCALL("syscall");
static const string TOK_RETURN("return");


// ------------------------------------------------------------------------
// utrace user-space probes
// ------------------------------------------------------------------------

// Note that these flags don't match up exactly with UTRACE_EVENT
// flags (and that's OK).
enum utrace_derived_probe_flags {
  UDPF_NONE,
  UDPF_BEGIN,				// process begin
  UDPF_END,				// process end
  UDPF_THREAD_BEGIN,			// thread begin
  UDPF_THREAD_END,			// thread end
  UDPF_SYSCALL,				// syscall entry
  UDPF_SYSCALL_RETURN,			// syscall exit
  UDPF_NFLAGS
};

struct utrace_derived_probe: public derived_probe
{
  bool has_path;
  string path;
  bool has_library;
  string library;
  int64_t pid;
  enum utrace_derived_probe_flags flags;
  bool target_symbol_seen;

  utrace_derived_probe (systemtap_session &s, probe* p, probe_point* l,
			bool hp, string &pn, int64_t pd,
			enum utrace_derived_probe_flags f);
  void join_group (systemtap_session& s);

  void emit_privilege_assertion (translator_output*);
  void print_dupe_stamp(ostream& o);
  void getargs (std::list<std::string> &arg_set) const;
};


struct utrace_derived_probe_group: public generic_dpg<utrace_derived_probe>
{
private:
  map<string, vector<utrace_derived_probe*> > probes_by_path;
  typedef map<string, vector<utrace_derived_probe*> >::iterator p_b_path_iterator;
  map<int64_t, vector<utrace_derived_probe*> > probes_by_pid;
  typedef map<int64_t, vector<utrace_derived_probe*> >::iterator p_b_pid_iterator;
  unsigned num_probes;
  bool flags_seen[UDPF_NFLAGS];

  // Using the linux backend
  void emit_linux_probe_decl (systemtap_session& s, utrace_derived_probe *p);
  void emit_module_linux_decls (systemtap_session& s);
  void emit_module_linux_init (systemtap_session& s);
  void emit_module_linux_exit (systemtap_session& s);

  // Using the dyninst backend (via stapdyn)
  void emit_dyninst_probe_decl (systemtap_session& s, const string& path,
				utrace_derived_probe *p);
  void emit_module_dyninst_decls (systemtap_session& s);
  void emit_module_dyninst_init (systemtap_session& s);
  void emit_module_dyninst_exit (systemtap_session& s);

public:
  utrace_derived_probe_group(): num_probes(0), flags_seen() { }

  void enroll (utrace_derived_probe* probe);
  void emit_module_decls (systemtap_session& s);
  void emit_module_init (systemtap_session& s);
  void emit_module_exit (systemtap_session& s);
};


struct utrace_var_expanding_visitor: public var_expanding_visitor
{
  utrace_var_expanding_visitor(systemtap_session& s, probe_point* l,
			       const string& pn,
                               enum utrace_derived_probe_flags f):
    sess (s), base_loc (l), probe_name (pn), flags (f),
    target_symbol_seen (false), add_block(NULL), add_probe(NULL) {}

  systemtap_session& sess;
  probe_point* base_loc;
  string probe_name;
  enum utrace_derived_probe_flags flags;
  bool target_symbol_seen;
  block *add_block;
  probe *add_probe;
  std::map<std::string, symbol *> return_ts_map;

  void visit_target_symbol_arg (target_symbol* e);
  void visit_target_symbol_context (target_symbol* e);
  void visit_target_symbol_cached (target_symbol* e);
  void visit_target_symbol (target_symbol* e);
};



utrace_derived_probe::utrace_derived_probe (systemtap_session &s,
                                            probe* p, probe_point* l,
					    bool hp, string &pn, int64_t pd,
					    enum utrace_derived_probe_flags f):
  derived_probe (p, l, true /* .components soon rewritten */ ),
  has_path(hp), path(pn), has_library(false), pid(pd), flags(f),
  target_symbol_seen(false)
{
  check_process_probe_kernel_support(s);

  // Expand local variables in the probe body
  utrace_var_expanding_visitor v (s, l, name, flags);
  v.replace (this->body);
  target_symbol_seen = v.target_symbol_seen;

  // If during target-variable-expanding the probe, we added a new block
  // of code, add it to the start of the probe.
  if (v.add_block)
    this->body = new block(v.add_block, this->body);
  // If when target-variable-expanding the probe, we added a new
  // probe, add it in a new file to the list of files to be processed.
  if (v.add_probe)
    {
      stapfile *f = new stapfile;
      f->probes.push_back(v.add_probe);
      s.files.push_back(f);
    }

  // Reset the sole element of the "locations" vector as a
  // "reverse-engineered" form of the incoming (q.base_loc) probe
  // point.  This allows a user to see what program etc.
  // number any particular match of the wildcards.

  vector<probe_point::component*> comps;
  if (hp)
    comps.push_back (new probe_point::component(TOK_PROCESS, new literal_string(path)));
  else if (pid != 0)
    comps.push_back (new probe_point::component(TOK_PROCESS, new literal_number(pid)));
  else
    comps.push_back (new probe_point::component(TOK_PROCESS));

  switch (flags)
    {
    case UDPF_THREAD_BEGIN:
      comps.push_back (new probe_point::component(TOK_THREAD));
      comps.push_back (new probe_point::component(TOK_BEGIN));
      break;
    case UDPF_THREAD_END:
      comps.push_back (new probe_point::component(TOK_THREAD));
      comps.push_back (new probe_point::component(TOK_END));
      break;
    case UDPF_SYSCALL:
      comps.push_back (new probe_point::component(TOK_SYSCALL));
      break;
    case UDPF_SYSCALL_RETURN:
      comps.push_back (new probe_point::component(TOK_SYSCALL));
      comps.push_back (new probe_point::component(TOK_RETURN));
      break;
    case UDPF_BEGIN:
      comps.push_back (new probe_point::component(TOK_BEGIN));
      break;
    case UDPF_END:
      comps.push_back (new probe_point::component(TOK_END));
      break;
    default:
      assert (0);
    }

  // Overwrite it.
  this->sole_location()->components = comps;
}


void
utrace_derived_probe::join_group (systemtap_session& s)
{
  if (! s.utrace_derived_probes)
    {
      s.utrace_derived_probes = new utrace_derived_probe_group ();
    }
  s.utrace_derived_probes->enroll (this);

  if (s.runtime_usermode_p())
    enable_dynprobes(s);
  else
    enable_task_finder(s);
}


void
utrace_derived_probe::emit_privilege_assertion (translator_output* o)
{
  // Process end probes can fire for unprivileged users even if the process
  // does not belong to the user. On example is that process.end will fire
  // at the end of a process which executes execve on an executable which
  // has the setuid bit set. When the setuid executable ends, the process.end
  // will fire even though the owner of the process is different than the
  // original owner.
  // Unprivileged users must use check is_myproc() from within any
  // process.end variant in their script before doing anything "dangerous".
  if (flags == UDPF_END)
    return;

  // Other process probes should only fire for unprivileged users in the
  // context of processes which they own. Generate an assertion to this effect
  // as a safety net.
  emit_process_owner_assertion (o);
}

void
utrace_derived_probe::print_dupe_stamp(ostream& o)
{
  // Process end probes can fire for unprivileged users even if the process
  // does not belong to the user. On example is that process.end will fire
  // at the end of a process which executes execve on an executable which
  // has the setuid bit set. When the setuid executable ends, the process.end
  // will fire even though the owner of the process is different than the
  // original owner.
  // Unprivileged users must use check is_myproc() from within any
  // process.end variant in their script before doing anything "dangerous".
  //
  // Other process probes should only fire for unprivileged users in the
  // context of processes which they own.
  if (flags == UDPF_END)
    print_dupe_stamp_unprivileged (o);
  else
    print_dupe_stamp_unprivileged_process_owner (o);
}

void
utrace_derived_probe::getargs(std::list<std::string> &arg_set) const
{
  arg_set.push_back("$syscall:long");
  arg_set.push_back("$arg1:long");
  arg_set.push_back("$arg2:long");
  arg_set.push_back("$arg3:long");
  arg_set.push_back("$arg4:long");
  arg_set.push_back("$arg5:long");
  arg_set.push_back("$arg6:long");
}

void
utrace_var_expanding_visitor::visit_target_symbol_cached (target_symbol* e)
{
      // Get the full name of the target symbol.
      stringstream ts_name_stream;
      e->print(ts_name_stream);
      string ts_name = ts_name_stream.str();

      // Check and make sure we haven't already seen this target
      // variable in this return probe.  If we have, just return our
      // last replacement.
      map<string, symbol *>::iterator i = return_ts_map.find(ts_name);
      if (i != return_ts_map.end())
	{
	  provide (i->second);
	  return;
	}

      // We've got to do several things here to handle target
      // variables in return probes.

      // (1) Synthesize a global array which is the cache of the
      // target variable value.  We don't need a nesting level counter
      // like the dwarf_var_expanding_visitor::visit_target_symbol()
      // does since a particular thread can only be in one system
      // calls at a time. The array will look like this:
      //
      //   _utrace_tvar_{name}_{num}
      string aname = (string("_utrace_tvar_")
		      + e->sym_name()
		      + "_" + lex_cast(tick++));
      vardecl* vd = new vardecl;
      vd->name = aname;
      vd->tok = e->tok;
      sess.globals.push_back (vd);

      // (2) Create a new code block we're going to insert at the
      // beginning of this probe to get the cached value into a
      // temporary variable.  We'll replace the target variable
      // reference with the temporary variable reference.  The code
      // will look like this:
      //
      //   _utrace_tvar_tid = tid()
      //   _utrace_tvar_{name}_{num}_tmp
      //       = _utrace_tvar_{name}_{num}[_utrace_tvar_tid]
      //   delete _utrace_tvar_{name}_{num}[_utrace_tvar_tid]

      // (2a) Synthesize the tid temporary expression, which will look
      // like this:
      //
      //   _utrace_tvar_tid = tid()
      symbol* tidsym = new symbol;
      tidsym->name = string("_utrace_tvar_tid");
      tidsym->tok = e->tok;

      if (add_block == NULL)
        {
	   add_block = new block;
	   add_block->tok = e->tok;

	   // Synthesize a functioncall to grab the thread id.
	   functioncall* fc = new functioncall;
	   fc->tok = e->tok;
	   fc->function = string("tid");

	   // Assign the tid to '_utrace_tvar_tid'.
	   assignment* a = new assignment;
	   a->tok = e->tok;
	   a->op = "=";
	   a->left = tidsym;
	   a->right = fc;

	   expr_statement* es = new expr_statement;
	   es->tok = e->tok;
	   es->value = a;
	   add_block->statements.push_back (es);
	}

      // (2b) Synthesize an array reference and assign it to a
      // temporary variable (that we'll use as replacement for the
      // target variable reference).  It will look like this:
      //
      //   _utrace_tvar_{name}_{num}_tmp
      //       = _utrace_tvar_{name}_{num}[_utrace_tvar_tid]

      arrayindex* ai_tvar = new arrayindex;
      ai_tvar->tok = e->tok;

      symbol* sym = new symbol;
      sym->name = aname;
      sym->tok = e->tok;
      ai_tvar->base = sym;

      ai_tvar->indexes.push_back(tidsym);

      symbol* tmpsym = new symbol;
      tmpsym->name = aname + "_tmp";
      tmpsym->tok = e->tok;

      assignment* a = new assignment;
      a->tok = e->tok;
      a->op = "=";
      a->left = tmpsym;
      a->right = ai_tvar;

      expr_statement* es = new expr_statement;
      es->tok = e->tok;
      es->value = a;

      add_block->statements.push_back (es);

      // (2c) Delete the array value.  It will look like this:
      //
      //   delete _utrace_tvar_{name}_{num}[_utrace_tvar_tid]

      delete_statement* ds = new delete_statement;
      ds->tok = e->tok;
      ds->value = ai_tvar;
      add_block->statements.push_back (ds);

      // (3) We need an entry probe that saves the value for us in the
      // global array we created.  Create the entry probe, which will
      // look like this:
      //
      //   probe process(PATH_OR_PID).syscall {
      //     _utrace_tvar_tid = tid()
      //     _utrace_tvar_{name}_{num}[_utrace_tvar_tid] = ${param}
      //   }
      //
      // Why the temporary for tid()?  If we end up caching more
      // than one target variable, we can reuse the temporary instead
      // of calling tid() multiple times.

      if (add_probe == NULL)
        {
	   add_probe = new probe;
	   add_probe->tok = e->tok;

	   // We need the name of the current probe point, minus the
	   // ".return".  Create a new probe point, copying all the
	   // components, stopping when we see the ".return"
	   // component.
	   probe_point* pp = new probe_point;
	   for (unsigned c = 0; c < base_loc->components.size(); c++)
	     {
	        if (base_loc->components[c]->functor == "return")
		  break;
	        else
		  pp->components.push_back(base_loc->components[c]);
	     }
	   pp->optional = base_loc->optional;
	   add_probe->locations.push_back(pp);

	   add_probe->body = new block;
	   add_probe->body->tok = e->tok;

	   // Synthesize a functioncall to grab the thread id.
	   functioncall* fc = new functioncall;
	   fc->tok = e->tok;
	   fc->function = string("tid");

	   // Assign the tid to '_utrace_tvar_tid'.
	   assignment* a = new assignment;
	   a->tok = e->tok;
	   a->op = "=";
	   a->left = tidsym;
	   a->right = fc;

	   expr_statement* es = new expr_statement;
	   es->tok = e->tok;
	   es->value = a;
           add_probe->body = new block(add_probe->body, es);

	   vardecl* vd = new vardecl;
	   vd->tok = e->tok;
	   vd->name = tidsym->name;
	   vd->type = pe_long;
	   vd->set_arity(0, e->tok);
	   add_probe->locals.push_back(vd);
	}

      // Save the value, like this:
      //
      //   _utrace_tvar_{name}_{num}[_utrace_tvar_tid] = ${param}
      a = new assignment;
      a->tok = e->tok;
      a->op = "=";
      a->left = ai_tvar;
      a->right = e;

      es = new expr_statement;
      es->tok = e->tok;
      es->value = a;

      add_probe->body = new block(add_probe->body, es);

      // (4) Provide the '_utrace_tvar_{name}_{num}_tmp' variable to
      // our parent so it can be used as a substitute for the target
      // symbol.
      provide (tmpsym);

      // (5) Remember this replacement since we might be able to reuse
      // it later if the same return probe references this target
      // symbol again.
      return_ts_map[ts_name] = tmpsym;
      return;
}


void
utrace_var_expanding_visitor::visit_target_symbol_arg (target_symbol* e)
{
  if (flags != UDPF_SYSCALL)
    throw semantic_error (_("only \"process(PATH_OR_PID).syscall\" support $argN or $$parms."), e->tok);

  if (e->name == "$$parms")
    {
      // copy from tracepoint
      token* pf_tok = new token(*e->tok);
      pf_tok->content = "sprintf";
      print_format* pf = print_format::create(pf_tok);

      target_symbol_seen = true;

      for (unsigned i = 0; i < 6; ++i)
        {
          if (i > 0)
            pf->raw_components += " ";
          pf->raw_components += "$arg" + lex_cast(i+1);
          target_symbol *tsym = new target_symbol;
          tsym->tok = e->tok;
          tsym->name = "$arg" + lex_cast(i+1);
          tsym->saved_conversion_error = 0;
          pf->raw_components += "=%#x"; //FIXME: missing type info

	  functioncall* n = new functioncall; //same as the following
	  n->tok = e->tok;
	  n->function = "_utrace_syscall_arg";
	  n->referent = 0;
	  literal_number *num = new literal_number(i);
	  num->tok = e->tok;
	  n->args.push_back(num);

          pf->args.push_back(n);
        }
      pf->components = print_format::string_to_components(pf->raw_components);

      provide (pf);
     }
   else // $argN
     {
        string argnum_s = e->name.substr(4,e->name.length()-4);
        int argnum = 0;
        try
          {
            argnum = lex_cast<int>(argnum_s);
          }
        catch (const runtime_error& f) // non-integral $arg suffix: e.g. $argKKKSDF
          {
           throw semantic_error (_("invalid syscall argument number (1-6)"), e->tok);
          }

        e->assert_no_components("utrace");

        // FIXME: max argnument number should not be hardcoded.
        if (argnum < 1 || argnum > 6)
           throw semantic_error (_("invalid syscall argument number (1-6)"), e->tok);

        bool lvalue = is_active_lvalue(e);
        if (lvalue)
           throw semantic_error(_("utrace '$argN' variable is read-only"), e->tok);

        // Remember that we've seen a target variable.
        target_symbol_seen = true;

        // We're going to substitute a synthesized '_utrace_syscall_arg'
        // function call for the '$argN' reference.
        functioncall* n = new functioncall;
        n->tok = e->tok;
        n->function = "_utrace_syscall_arg";
        n->referent = 0; // NB: must not resolve yet, to ensure inclusion in session

        literal_number *num = new literal_number(argnum - 1);
        num->tok = e->tok;
        n->args.push_back(num);

        provide (n);
     }
}

void
utrace_var_expanding_visitor::visit_target_symbol_context (target_symbol* e)
{
  const string& sname = e->name;

  e->assert_no_components("utrace");

  bool lvalue = is_active_lvalue(e);
  if (lvalue)
    throw semantic_error(_F("utrace '%s' variable is read-only", sname.c_str()), e->tok);

  string fname;
  if (sname == "$return")
    {
      if (flags != UDPF_SYSCALL_RETURN)
	throw semantic_error (_("only \"process(PATH_OR_PID).syscall.return\" support $return."), e->tok);
      fname = "_utrace_syscall_return";
    }
  else if (sname == "$syscall")
    {
      // If we've got a syscall entry probe, we can just call the
      // right function.
      if (flags == UDPF_SYSCALL) {
        fname = "_utrace_syscall_nr";
      }
      // If we're in a syscal return probe, we can't really access
      // $syscall.  So, similar to what
      // dwarf_var_expanding_visitor::visit_target_symbol() does,
      // we'll create an syscall entry probe to cache $syscall, then
      // we'll access the cached value in the syscall return probe.
      else {
	visit_target_symbol_cached (e);

	// Remember that we've seen a target variable.
	target_symbol_seen = true;
	return;
      }
    }
  else
    {
      throw semantic_error (_("unknown target variable"), e->tok);
    }

  // Remember that we've seen a target variable.
  target_symbol_seen = true;

  // We're going to substitute a synthesized '_utrace_syscall_nr'
  // function call for the '$syscall' reference.
  functioncall* n = new functioncall;
  n->tok = e->tok;
  n->function = fname;
  n->referent = 0; // NB: must not resolve yet, to ensure inclusion in session

  provide (n);
}

void
utrace_var_expanding_visitor::visit_target_symbol (target_symbol* e)
{
  assert(e->name.size() > 0 && e->name[0] == '$');

  try
    {
      if (flags != UDPF_SYSCALL && flags != UDPF_SYSCALL_RETURN)
        throw semantic_error (_("only \"process(PATH_OR_PID).syscall\""
                                " and \"process(PATH_OR_PID).syscall.return\" probes support target symbols"),
                              e->tok);

      if (e->addressof)
        throw semantic_error(_("cannot take address of utrace variable"), e->tok);

      if (startswith(e->name, "$arg") || e->name == "$$parms")
        visit_target_symbol_arg(e);
      else if (e->name == "$syscall" || e->name == "$return")
        visit_target_symbol_context(e);
      else
        throw semantic_error (_("invalid target symbol for utrace probe,"
                                " $syscall, $return, $argN or $$parms expected"),
                              e->tok);
    }
  catch (const semantic_error &er)
    {
      e->chain (er);
      provide(e);
      return;
    }
}


struct utrace_builder: public derived_probe_builder
{
  utrace_builder() {}
  virtual void build(systemtap_session & sess,
		     probe * base,
		     probe_point * location,
		     literal_map_t const & parameters,
		     vector<derived_probe *> & finished_results)
  {
    string path, path_tgt;
    int64_t pid;

    bool has_path = get_param (parameters, TOK_PROCESS, path);
    bool has_pid = get_param (parameters, TOK_PROCESS, pid);
    enum utrace_derived_probe_flags flags = UDPF_NONE;

    if (has_null_param (parameters, TOK_THREAD))
      {
	if (has_null_param (parameters, TOK_BEGIN))
	  flags = UDPF_THREAD_BEGIN;
	else if (has_null_param (parameters, TOK_END))
	  flags = UDPF_THREAD_END;
      }
    else if (has_null_param (parameters, TOK_SYSCALL))
      {
	if (sess.runtime_usermode_p())
	  throw semantic_error (_("process.syscall probes not available with the dyninst runtime"));

	if (has_null_param (parameters, TOK_RETURN))
	  flags = UDPF_SYSCALL_RETURN;
	else
	  flags = UDPF_SYSCALL;
      }
    else if (has_null_param (parameters, TOK_BEGIN))
      flags = UDPF_BEGIN;
    else if (has_null_param (parameters, TOK_END))
      flags = UDPF_END;

    // If we didn't get a path or pid, this means to probe everything.
    // Convert this to a pid-based probe.
    if (! has_path && ! has_pid)
      {
	has_path = false;
	path.clear();
	has_pid = true;
	pid = 0;
      }
    else if (has_path)
      {
        path = find_executable (path, sess.sysroot, sess.sysenv);
        sess.unwindsym_modules.insert (path);
        path_tgt = path_remove_sysroot(sess, path);
      }
    else if (has_pid)
      {
	// We can't probe 'init' (pid 1).  XXX: where does this limitation come from?
	if (pid < 2)
	  throw semantic_error (_("process pid must be greater than 1"),
				location->components.front()->tok);

        // XXX: could we use /proc/$pid/exe in unwindsym_modules and elsewhere?
      }

    finished_results.push_back(new utrace_derived_probe(sess, base, location,
							has_path, path_tgt, pid,
							flags));
  }
};


void
utrace_derived_probe_group::enroll (utrace_derived_probe* p)
{
  if (p->has_path)
    probes_by_path[p->path].push_back(p);
  else
    probes_by_pid[p->pid].push_back(p);
  num_probes++;
  flags_seen[p->flags] = true;

  // XXX: multiple exec probes (for instance) for the same path (or
  // pid) should all share a utrace report function, and have their
  // handlers executed sequentially.
}


void
utrace_derived_probe_group::emit_linux_probe_decl (systemtap_session& s,
						   utrace_derived_probe *p)
{
  s.op->newline() << "{";
  s.op->line() << " .tgt={";
  s.op->line() << " .purpose=\"lifecycle tracking\",";
  if (p->has_path)
    {
      s.op->line() << " .procname=\"" << p->path << "\",";
      s.op->line() << " .pid=0,";
    }
  else
    {
      s.op->line() << " .procname=NULL,";
      s.op->line() << " .pid=" << p->pid << ",";
    }

  s.op->line() << " .callback=&_stp_utrace_probe_cb,";
  s.op->line() << " .mmap_callback=NULL,";
  s.op->line() << " .munmap_callback=NULL,";
  s.op->line() << " .mprotect_callback=NULL,";
  s.op->line() << " },";
  s.op->line() << " .probe=" << common_probe_init (p) << ",";

  // Handle flags
  switch (p->flags)
    {
    // Notice that we'll just call the probe directly when we get
    // notified, since the task_finder layer stops the thread for us.
    case UDPF_BEGIN:				// process begin
      s.op->line() << " .flags=(UDPF_BEGIN),";
      break;
    case UDPF_THREAD_BEGIN:			// thread begin
      s.op->line() << " .flags=(UDPF_THREAD_BEGIN),";
      break;

    // Notice we're not setting up a .ops/.report_death handler for
    // either UDPF_END or UDPF_THREAD_END.  Instead, we'll just call
    // the probe directly when we get notified.
    case UDPF_END:				// process end
      s.op->line() << " .flags=(UDPF_END),";
      break;
    case UDPF_THREAD_END:			// thread end
      s.op->line() << " .flags=(UDPF_THREAD_END),";
      break;

    // For UDPF_SYSCALL/UDPF_SYSCALL_RETURN probes, the .report_death
    // handler isn't strictly necessary.  However, it helps to keep
    // our attaches/detaches symmetrical.  Since the task_finder layer
    // stops the thread, that works around bug 6841.
    case UDPF_SYSCALL:
      s.op->line() << " .flags=(UDPF_SYSCALL),";
      s.op->line() << " .ops={ .report_syscall_entry=stap_utrace_probe_syscall,  .report_death=stap_utrace_task_finder_report_death },";
      s.op->line() << " .events=(UTRACE_EVENT(SYSCALL_ENTRY)|UTRACE_EVENT(DEATH)),";
      break;
    case UDPF_SYSCALL_RETURN:
      s.op->line() << " .flags=(UDPF_SYSCALL_RETURN),";
      s.op->line() << " .ops={ .report_syscall_exit=stap_utrace_probe_syscall, .report_death=stap_utrace_task_finder_report_death },";
      s.op->line() << " .events=(UTRACE_EVENT(SYSCALL_EXIT)|UTRACE_EVENT(DEATH)),";
      break;

    case UDPF_NONE:
      s.op->line() << " .flags=(UDPF_NONE),";
      s.op->line() << " .ops={ },";
      s.op->line() << " .events=0,";
      break;
    default:
      throw semantic_error ("bad utrace probe flag");
      break;
    }
  s.op->line() << " .engine_attached=0,";
  s.op->line() << " },";
}


void
utrace_derived_probe_group::emit_module_linux_decls (systemtap_session& s)
{
  if (probes_by_path.empty() && probes_by_pid.empty())
    return;

  s.op->newline();
  s.op->newline() << "/* ---- utrace probes ---- */";

  s.op->newline() << "enum utrace_derived_probe_flags {";
  s.op->indent(1);
  s.op->newline() << "UDPF_NONE,";
  s.op->newline() << "UDPF_BEGIN,";
  s.op->newline() << "UDPF_END,";
  s.op->newline() << "UDPF_THREAD_BEGIN,";
  s.op->newline() << "UDPF_THREAD_END,";
  s.op->newline() << "UDPF_SYSCALL,";
  s.op->newline() << "UDPF_SYSCALL_RETURN,";
  s.op->newline() << "UDPF_NFLAGS";
  s.op->newline(-1) << "};";

  s.op->newline() << "struct stap_utrace_probe {";
  s.op->indent(1);
  s.op->newline() << "struct stap_task_finder_target tgt;";
  s.op->newline() << "const struct stap_probe * const probe;";
  s.op->newline() << "int engine_attached;";
  s.op->newline() << "enum utrace_derived_probe_flags flags;";
  s.op->newline() << "struct utrace_engine_ops ops;";
  s.op->newline() << "unsigned long events;";
  s.op->newline(-1) << "};";


  // Output handler function for UDPF_BEGIN, UDPF_THREAD_BEGIN,
  // UDPF_END, and UDPF_THREAD_END
  if (flags_seen[UDPF_BEGIN] || flags_seen[UDPF_THREAD_BEGIN]
      || flags_seen[UDPF_END] || flags_seen[UDPF_THREAD_END])
    {
      s.op->newline() << "static void stap_utrace_probe_handler(struct task_struct *tsk, struct stap_utrace_probe *p) {";
      s.op->indent(1);

      common_probe_entryfn_prologue (s, "STAP_SESSION_RUNNING", "p->probe",
				     "stp_probe_type_utrace");

      // call probe function
      s.op->newline() << "(*p->probe->ph) (c);";
      common_probe_entryfn_epilogue (s, true);

      s.op->newline() << "return;";
      s.op->newline(-1) << "}";
    }

  // Output handler function for SYSCALL_ENTRY and SYSCALL_EXIT events
  if (flags_seen[UDPF_SYSCALL] || flags_seen[UDPF_SYSCALL_RETURN])
    {
      s.op->newline() << "#ifdef UTRACE_ORIG_VERSION";
      s.op->newline() << "static u32 stap_utrace_probe_syscall(struct utrace_engine *engine, struct task_struct *tsk, struct pt_regs *regs) {";
      s.op->newline() << "#else";
      s.op->newline() << "#if defined(UTRACE_API_VERSION) && (UTRACE_API_VERSION >= 20091216)";
      s.op->newline() << "static u32 stap_utrace_probe_syscall(u32 action, struct utrace_engine *engine, struct pt_regs *regs) {";
      s.op->newline() << "#else";
      s.op->newline() << "static u32 stap_utrace_probe_syscall(enum utrace_resume_action action, struct utrace_engine *engine, struct task_struct *tsk, struct pt_regs *regs) {";
      s.op->newline() << "#endif";
      s.op->newline() << "#endif";

      s.op->indent(1);
      s.op->newline() << "struct stap_utrace_probe *p = (struct stap_utrace_probe *)engine->data;";

      common_probe_entryfn_prologue (s, "STAP_SESSION_RUNNING", "p->probe",
				     "stp_probe_type_utrace_syscall");
      s.op->newline() << "c->uregs = regs;";
      s.op->newline() << "c->user_mode_p = 1;";

      // call probe function
      s.op->newline() << "(*p->probe->ph) (c);";
      common_probe_entryfn_epilogue (s, true);

      s.op->newline() << "if ((atomic_read (session_state()) != STAP_SESSION_STARTING) && (atomic_read (session_state()) != STAP_SESSION_RUNNING)) {";
      s.op->indent(1);
      s.op->newline() << "debug_task_finder_detach();";
      s.op->newline() << "return UTRACE_DETACH;";
      s.op->newline(-1) << "}";
      s.op->newline() << "return UTRACE_RESUME;";
      s.op->newline(-1) << "}";
    }

  // Output task_finder callback routine that gets called for all
  // utrace probe types.
  s.op->newline() << "static int _stp_utrace_probe_cb(struct stap_task_finder_target *tgt, struct task_struct *tsk, int register_p, int process_p) {";
  s.op->indent(1);
  s.op->newline() << "int rc = 0;";
  s.op->newline() << "struct stap_utrace_probe *p = container_of(tgt, struct stap_utrace_probe, tgt);";
  s.op->newline() << "struct utrace_engine *engine;";

  s.op->newline() << "if (register_p) {";
  s.op->indent(1);

  s.op->newline() << "switch (p->flags) {";
  s.op->indent(1);

  // When receiving a UTRACE_EVENT(CLONE) event, we can't call the
  // begin/thread.begin probe directly.  So, we'll just attach an
  // engine that waits for the thread to quiesce.  When the thread
  // quiesces, then call the probe.
  if (flags_seen[UDPF_BEGIN])
  {
      s.op->newline() << "case UDPF_BEGIN:";
      s.op->indent(1);
      s.op->newline() << "if (process_p) {";
      s.op->indent(1);
      s.op->newline() << "stap_utrace_probe_handler(tsk, p);";
      s.op->newline(-1) << "}";
      s.op->newline() << "break;";
      s.op->indent(-1);
  }
  if (flags_seen[UDPF_THREAD_BEGIN])
  {
      s.op->newline() << "case UDPF_THREAD_BEGIN:";
      s.op->indent(1);
      s.op->newline() << "if (! process_p) {";
      s.op->indent(1);
      s.op->newline() << "stap_utrace_probe_handler(tsk, p);";
      s.op->newline(-1) << "}";
      s.op->newline() << "break;";
      s.op->indent(-1);
  }

  // For end/thread_end probes, do nothing at registration time.
  // We'll handle these in the 'register_p == 0' case.
  if (flags_seen[UDPF_END] || flags_seen[UDPF_THREAD_END])
    {
      s.op->newline() << "case UDPF_END:";
      s.op->newline() << "case UDPF_THREAD_END:";
      s.op->indent(1);
      s.op->newline() << "break;";
      s.op->indent(-1);
    }

  // Attach an engine for SYSCALL_ENTRY and SYSCALL_EXIT events.
  if (flags_seen[UDPF_SYSCALL] || flags_seen[UDPF_SYSCALL_RETURN])
    {
      s.op->newline() << "case UDPF_SYSCALL:";
      s.op->newline() << "case UDPF_SYSCALL_RETURN:";
      s.op->indent(1);
      s.op->newline() << "rc = stap_utrace_attach(tsk, &p->ops, p, p->events);";
      s.op->newline() << "if (rc == 0) {";
      s.op->indent(1);
      s.op->newline() << "p->engine_attached = 1;";
      s.op->newline(-1) << "}";
      s.op->newline() << "break;";
      s.op->indent(-1);
    }

  s.op->newline() << "default:";
  s.op->indent(1);
  s.op->newline() << "_stp_error(\"unhandled flag value %d at %s:%d\", p->flags, __FUNCTION__, __LINE__);";
  s.op->newline() << "break;";
  s.op->indent(-1);
  s.op->newline(-1) << "}";
  s.op->newline(-1) << "}";

  // Since this engine could be attached to multiple threads, don't
  // call stap_utrace_detach_ops() here, only call
  // stap_utrace_detach() as necessary.
  s.op->newline() << "else {";
  s.op->indent(1);
  s.op->newline() << "switch (p->flags) {";
  s.op->indent(1);
  // For end probes, go ahead and call the probe directly.
  if (flags_seen[UDPF_END])
    {
      s.op->newline() << "case UDPF_END:";
      s.op->indent(1);
      s.op->newline() << "if (process_p) {";
      s.op->indent(1);
      s.op->newline() << "stap_utrace_probe_handler(tsk, p);";
      s.op->newline(-1) << "}";
      s.op->newline() << "break;";
      s.op->indent(-1);
    }
  if (flags_seen[UDPF_THREAD_END])
    {
      s.op->newline() << "case UDPF_THREAD_END:";
      s.op->indent(1);
      s.op->newline() << "if (! process_p) {";
      s.op->indent(1);
      s.op->newline() << "stap_utrace_probe_handler(tsk, p);";
      s.op->newline(-1) << "}";
      s.op->newline() << "break;";
      s.op->indent(-1);
    }

  // For begin/thread_begin probes, we don't need to do anything.
  if (flags_seen[UDPF_BEGIN] || flags_seen[UDPF_THREAD_BEGIN])
  {
      s.op->newline() << "case UDPF_BEGIN:";
      s.op->newline() << "case UDPF_THREAD_BEGIN:";
      s.op->indent(1);
      s.op->newline() << "break;";
      s.op->indent(-1);
  }

  if (flags_seen[UDPF_SYSCALL] || flags_seen[UDPF_SYSCALL_RETURN])
    {
      s.op->newline() << "case UDPF_SYSCALL:";
      s.op->newline() << "case UDPF_SYSCALL_RETURN:";
      s.op->indent(1);
      s.op->newline() << "stap_utrace_detach(tsk, &p->ops);";
      s.op->newline() << "break;";
      s.op->indent(-1);
    }

  s.op->newline() << "default:";
  s.op->indent(1);
  s.op->newline() << "_stp_error(\"unhandled flag value %d at %s:%d\", p->flags, __FUNCTION__, __LINE__);";
  s.op->newline() << "break;";
  s.op->indent(-1);
  s.op->newline(-1) << "}";
  s.op->newline(-1) << "}";
  s.op->newline() << "return rc;";
  s.op->newline(-1) << "}";

  s.op->newline() << "static struct stap_utrace_probe stap_utrace_probes[] = {";
  s.op->indent(1);

  // Set up 'process(PATH)' probes
  if (! probes_by_path.empty())
    {
      for (p_b_path_iterator it = probes_by_path.begin();
	   it != probes_by_path.end(); it++)
        {
	  for (unsigned i = 0; i < it->second.size(); i++)
	    {
	      utrace_derived_probe *p = it->second[i];
	      emit_linux_probe_decl(s, p);
	    }
	}
    }

  // Set up 'process(PID)' probes
  if (! probes_by_pid.empty())
    {
      for (p_b_pid_iterator it = probes_by_pid.begin();
	   it != probes_by_pid.end(); it++)
        {
	  for (unsigned i = 0; i < it->second.size(); i++)
	    {
	      utrace_derived_probe *p = it->second[i];
	      emit_linux_probe_decl(s, p);
	    }
	}
    }
  s.op->newline(-1) << "};";
}


void
utrace_derived_probe_group::emit_dyninst_probe_decl (systemtap_session& s,
						     const string& path,
						     utrace_derived_probe *p)
{
  string flags_str;

  // Handle flags
  switch (p->flags)
    {
    case UDPF_BEGIN:	// process begin
      flags_str = "STAPDYN_PROBE_FLAG_PROC_BEGIN";
      break;
    case UDPF_THREAD_BEGIN:	// thread begin
      flags_str = "STAPDYN_PROBE_FLAG_THREAD_BEGIN";
      break;
    case UDPF_END:		// process end
      flags_str = "STAPDYN_PROBE_FLAG_PROC_END";
      break;
    case UDPF_THREAD_END:	// thread end
      flags_str = "STAPDYN_PROBE_FLAG_THREAD_END";
      break;

      // FIXME: No handling of syscall probes for dyninst yet.
#if 0
    case UDPF_SYSCALL:
      break;
    case UDPF_SYSCALL_RETURN:
      break;

    case UDPF_NONE:
      s.op->line() << " .flags=(UDPF_NONE),";
      s.op->line() << " .ops={ },";
      s.op->line() << " .events=0,";
      break;
#endif
    default:
      throw semantic_error ("bad utrace probe flag");
      break;
    }

  if (p->has_path)
    dynprobe_add_utrace_path(s, path, flags_str, common_probe_init(p));
  else
    dynprobe_add_utrace_pid(s, p->pid, flags_str, common_probe_init(p));
}

void
utrace_derived_probe_group::emit_module_dyninst_decls (systemtap_session& s)
{
  if (probes_by_path.empty() && probes_by_pid.empty())
    return;

  s.op->newline();
  s.op->newline() << "/* ---- dyninst utrace probes ---- */";
  s.op->newline() << "#include \"dyninst/uprobes.h\"";
  s.op->newline() << "#define STAPDYN_UTRACE_PROBES";

  // Let the dynprobe_derived_probe_group handle outputting targets
  // and probes. This allows us to merge different types of probes.
  s.op->newline() << "static struct stapdu_probe stapdu_probes[];";

  // Set up 'process(PATH)' probes
  if (! probes_by_path.empty())
    {
      for (p_b_path_iterator it = probes_by_path.begin();
	   it != probes_by_path.end(); it++)
        {
	  for (unsigned i = 0; i < it->second.size(); i++)
	    {
	      utrace_derived_probe *p = it->second[i];
	      emit_dyninst_probe_decl(s, it->first, p);
	    }
	}
    }
  // Set up 'process(PID)' probes
  if (! probes_by_pid.empty())
    {
      for (p_b_pid_iterator it = probes_by_pid.begin();
	   it != probes_by_pid.end(); it++)
        {
	  for (unsigned i = 0; i < it->second.size(); i++)
	    {
	      utrace_derived_probe *p = it->second[i];
	      emit_dyninst_probe_decl(s, "", p);
	    }
	}
    }

  // loc2c-generated code assumes pt_regs are available, so use this to make
  // sure we always have *something* for it to dereference...
  s.op->newline() << "static struct pt_regs stapdu_dummy_uregs;";

  // Write the probe handler.
  // NB: not static, so dyninst can find it
  s.op->newline() << "int enter_dyninst_utrace_probe "
                  << "(uint64_t index, struct pt_regs *regs) {";
  s.op->newline(1) << "struct stapdu_probe *sup = &stapdu_probes[index];";

  common_probe_entryfn_prologue (s, "STAP_SESSION_RUNNING", "sup->probe",
                                 "stp_probe_type_utrace");
  s.op->newline() << "c->uregs = regs ?: &stapdu_dummy_uregs;";
  s.op->newline() << "c->user_mode_p = 1;";
  // XXX: once we have regs, check how dyninst sets the IP
  // XXX: the way that dyninst rewrites stuff is probably going to be
  // ...  very confusing to our backtracer (at least if we stay in process)
  s.op->newline() << "(*sup->probe->ph) (c);";
  common_probe_entryfn_epilogue (s, true);
  s.op->newline() << "return 0;";
  s.op->newline(-1) << "}";
  s.op->assert_0_indent();
}


void
utrace_derived_probe_group::emit_module_decls (systemtap_session& s)
{
  if (s.runtime_usermode_p())
    emit_module_dyninst_decls (s);
  else
    emit_module_linux_decls (s);
}


void
utrace_derived_probe_group::emit_module_linux_init (systemtap_session& s)
{
  if (probes_by_path.empty() && probes_by_pid.empty())
    return;

  s.op->newline() << "/* ---- utrace probes ---- */";
  s.op->newline() << "for (i=0; i<ARRAY_SIZE(stap_utrace_probes); i++) {";
  s.op->newline(1) << "struct stap_utrace_probe *p = &stap_utrace_probes[i];";
  s.op->newline() << "probe_point = p->probe->pp;"; // for error messages
  s.op->newline() << "rc = stap_register_task_finder_target(&p->tgt);";

  // NB: if (rc), there is no need (XXX: nor any way) to clean up any
  // finders already registered, since mere registration does not
  // cause any utrace or memory allocation actions.  That happens only
  // later, once the task finder engine starts running.  So, for a
  // partial initialization requiring unwind, we need do nothing.
  s.op->newline() << "if (rc) break;";

  s.op->newline(-1) << "}";
}


void
utrace_derived_probe_group::emit_module_dyninst_init (systemtap_session& s)
{
  if (probes_by_path.empty() && probes_by_pid.empty())
    return;

  /* stapdyn handles the dirty work via dyninst */
  s.op->newline() << "/* ---- dyninst utrace probes ---- */";
  s.op->newline() << "/* this section left intentionally blank */";
}


void
utrace_derived_probe_group::emit_module_init (systemtap_session& s)
{
  if (s.runtime_usermode_p())
    emit_module_dyninst_init (s);
  else
    emit_module_linux_init(s);
}


void
utrace_derived_probe_group::emit_module_linux_exit (systemtap_session& s)
{
  if (probes_by_path.empty() && probes_by_pid.empty()) return;

  s.op->newline();
  s.op->newline() << "/* ---- utrace probes ---- */";
  s.op->newline() << "for (i=0; i<ARRAY_SIZE(stap_utrace_probes); i++) {";
  s.op->newline(1) << "struct stap_utrace_probe *p = &stap_utrace_probes[i];";

  s.op->newline() << "if (p->engine_attached) {";
  s.op->newline(1) << "stap_utrace_detach_ops(&p->ops);";

  s.op->newline(-1) << "}";
  s.op->newline(-1) << "}";
}


void
utrace_derived_probe_group::emit_module_dyninst_exit (systemtap_session& s)
{
  if (probes_by_path.empty() && probes_by_pid.empty())
    return;

  /* stapdyn handles the dirty work via dyninst */
  s.op->newline() << "/* ---- dyninst utrace probes ---- */";
  s.op->newline() << "/* this section left intentionally blank */";
}


void
utrace_derived_probe_group::emit_module_exit (systemtap_session& s)
{
  if (s.runtime_usermode_p())
    emit_module_dyninst_exit (s);
  else
    emit_module_linux_exit(s);
}


void
register_tapset_utrace(systemtap_session& s)
{
  match_node* root = s.pattern_root;
  derived_probe_builder *builder = new utrace_builder();

  vector<match_node*> roots;
  roots.push_back(root->bind(TOK_PROCESS));
  roots.push_back(root->bind_str(TOK_PROCESS));
  roots.push_back(root->bind_num(TOK_PROCESS));

  for (unsigned i = 0; i < roots.size(); ++i)
    {
      roots[i]->bind(TOK_BEGIN)
	->bind_privilege(pr_all)
	->bind(builder);
      roots[i]->bind(TOK_END)
	->bind_privilege(pr_all)
	->bind(builder);
      roots[i]->bind(TOK_THREAD)->bind(TOK_BEGIN)
	->bind_privilege(pr_all)
	->bind(builder);
      roots[i]->bind(TOK_THREAD)->bind(TOK_END)
	->bind_privilege(pr_all)
	->bind(builder);
      roots[i]->bind(TOK_SYSCALL)
	->bind_privilege(pr_all)
	->bind(builder);
      roots[i]->bind(TOK_SYSCALL)->bind(TOK_RETURN)
	->bind_privilege(pr_all)
	->bind(builder);
    }
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
