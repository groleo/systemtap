// tapset for HW performance monitoring
// Copyright (C) 2005-2012 Red Hat Inc.
// Copyright (C) 2005-2007 Intel Corporation.
// Copyright (C) 2008 James.Bottomley@HansenPartnership.com
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "session.h"
#include "tapsets.h"
#include "task_finder.h"
#include "translate.h"
#include "util.h"

#include <string>
#include <wordexp.h>

extern "C" {
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
}

using namespace std;
using namespace __gnu_cxx;


static const string TOK_PERF("perf");
static const string TOK_TYPE("type");
static const string TOK_CONFIG("config");
static const string TOK_SAMPLE("sample");
static const string TOK_PROCESS("process");


// ------------------------------------------------------------------------
// perf event derived probes
// ------------------------------------------------------------------------
// This is a new interface to the perfmon hw.
//

struct perf_derived_probe: public derived_probe
{
  int64_t event_type;
  int64_t event_config;
  int64_t interval;
  bool has_process;
  string process_name;
  perf_derived_probe (probe* p, probe_point* l, int64_t type, int64_t config, int64_t i, bool pp, string pn);
  virtual void join_group (systemtap_session& s);
};


struct perf_derived_probe_group: public generic_dpg<perf_derived_probe>
{
  void emit_module_decls (systemtap_session& s);
  void emit_module_init (systemtap_session& s);
  void emit_module_exit (systemtap_session& s);
};


perf_derived_probe::perf_derived_probe (probe* p, probe_point* l,
                                        int64_t type,
                                        int64_t config,
                                        int64_t i,
					bool process_p,
					string process_n):
  
  derived_probe (p, l, true /* .components soon rewritten */),
  event_type (type), event_config (config), interval (i),
  has_process (process_p), process_name (process_n)
{
  vector<probe_point::component*>& comps = this->sole_location()->components;
  comps.clear();
  comps.push_back (new probe_point::component (TOK_PERF));
  comps.push_back (new probe_point::component (TOK_TYPE, new literal_number(type)));
  comps.push_back (new probe_point::component (TOK_CONFIG, new literal_number (config)));
  comps.push_back (new probe_point::component (TOK_SAMPLE, new literal_number (interval)));
  comps.push_back (new probe_point::component (TOK_PROCESS, new literal_string (process_name)));
}


void
perf_derived_probe::join_group (systemtap_session& s)
{
  if (! s.perf_derived_probes)
    s.perf_derived_probes = new perf_derived_probe_group ();
  s.perf_derived_probes->enroll (this);

  enable_task_finder(s);

}


void
perf_derived_probe_group::emit_module_decls (systemtap_session& s)
{
  if (probes.empty()) return;

  s.op->newline() << "/* ---- perf probes ---- */";
  s.op->newline() << "#include <linux/perf_event.h>";
  s.op->newline() << "#include \"linux/perf.h\"";
  s.op->newline();

  /* declarations */
  s.op->newline() << "static void handle_perf_probe (unsigned i, struct pt_regs *regs);";
  for (unsigned i=0; i < probes.size(); i++)
    {
      s.op->newline() << "#ifdef STAPCONF_PERF_HANDLER_NMI";
      s.op->newline() << "static void enter_perf_probe_" << i
                      << " (struct perf_event *e, int nmi, "
                      << "struct perf_sample_data *data, "
                      << "struct pt_regs *regs);";
      s.op->newline() << "#else";
      s.op->newline() << "static void enter_perf_probe_" << i
                      << " (struct perf_event *e, "
                      << "struct perf_sample_data *data, "
                      << "struct pt_regs *regs);";
      s.op->newline() << "#endif";
    }
  s.op->newline();

  // Output task finder callback routine that gets called for all
  // perf probe types.
  s.op->newline() << "static int _stp_perf_probe_cb(struct stap_task_finder_target *tgt, struct task_struct *tsk, int register_p, int process_p) {";
  s.op->indent(1);
  s.op->newline() << "int rc = 0;";
  s.op->newline() << "struct stap_perf_probe *p = container_of(tgt, struct stap_perf_probe, tgt);";

  s.op->newline() << "_stp_printf(\"XXX %d %d\\n\", current->pid, task_pid_nr_ns(tsk,task_active_pid_ns(current->parent)));";
  s.op->newline() << "if (register_p) ";
  s.op->indent(1);

  s.op->newline() << "rc = _stp_perf_init(p, tsk);";
  s.op->newline(-1) << "else";
  s.op->newline(1) << "_stp_perf_del(p);";
  s.op->newline(-1) << "return rc;";
  s.op->newline(-1) << "}";

  /* data structures */
  s.op->newline() << "static struct stap_perf_probe stap_perf_probes ["
                  << probes.size() << "] = {";
  s.op->indent(1);
  for (unsigned i=0; i < probes.size(); i++)
    {
      s.op->newline() << "{";
      s.op->newline(1) << ".attr={ "
                       << ".type=" << probes[i]->event_type << "ULL, "
                       << ".config=" << probes[i]->event_config << "ULL, "
                       << "{ .sample_period=" << probes[i]->interval << "ULL }},";
      s.op->newline() << ".callback=enter_perf_probe_" << i << ", ";
      s.op->newline() << ".probe=" << common_probe_init (probes[i]) << ", ";

      string l_process_name;
      if (probes[i]->has_process)
	{
	  if (probes[i]->process_name.length() == 0)
	    {
	      wordexp_t words;
	      int rc = wordexp(s.cmd.c_str(), &words, WRDE_NOCMD|WRDE_UNDEF);
	      if (rc || words.we_wordc <= 0)
		throw semantic_error(_("unspecified process probe is invalid without a -c COMMAND"));
	      l_process_name = words.we_wordv[0];
	      wordfree (& words);
	    }
	  else
	    l_process_name = probes[i]->process_name;

	  s.op->line() << " .tgt={";
	  s.op->line() << " .procname=\"" << l_process_name << "\",";
	  s.op->line() << " .pid=0,";
	  s.op->line() << " .callback=&_stp_perf_probe_cb,";
	  s.op->line() << " },";
	  s.op->newline() << ".per_thread=" << "1, ";
	}
      else
	s.op->newline() << ".per_thread=" << "0, ";
      s.op->newline(-1) << "},";
    }
  s.op->newline(-1) << "};";
  s.op->newline();

  /* wrapper functions */
  for (unsigned i=0; i < probes.size(); i++)
    {
      s.op->newline() << "#ifdef STAPCONF_PERF_HANDLER_NMI";
      s.op->newline() << "static void enter_perf_probe_" << i
                      << " (struct perf_event *e, int nmi, "
                      << "struct perf_sample_data *data, "
                      << "struct pt_regs *regs)";
      s.op->newline() << "#else";
      s.op->newline() << "static void enter_perf_probe_" << i
                      << " (struct perf_event *e, "
                      << "struct perf_sample_data *data, "
                      << "struct pt_regs *regs)";
      s.op->newline() << "#endif";
      s.op->newline() << "{";
      s.op->newline(1) << "handle_perf_probe(" << i << ", regs);";
      s.op->newline(-1) << "}";
    }
  s.op->newline();

  s.op->newline() << "static void handle_perf_probe (unsigned i, struct pt_regs *regs)";
  s.op->newline() << "{";
  s.op->newline(1) << "struct stap_perf_probe* stp = & stap_perf_probes [i];";
  common_probe_entryfn_prologue (s, "STAP_SESSION_RUNNING", "stp->probe",
				 "stp_probe_type_perf");
  s.op->newline() << "if (user_mode(regs)) {";
  s.op->newline(1)<< "c->user_mode_p = 1;";
  s.op->newline() << "c->uregs = regs;";
  s.op->newline(-1) << "} else {";
  s.op->newline(1) << "c->kregs = regs;";
  s.op->newline(-1) << "}";

  s.op->newline() << "(*stp->probe->ph) (c);";
  common_probe_entryfn_epilogue (s, true);
  s.op->newline(-1) << "}";

  s.op->newline();
  s.op->newline() << "#include \"linux/perf.c\"";
  s.op->newline();
}


void
perf_derived_probe_group::emit_module_init (systemtap_session& s)
{
  if (probes.empty()) return;

  s.op->newline() << "for (i=0; i<" << probes.size() << "; i++) {";
  s.op->newline(1) << "struct stap_perf_probe* stp = & stap_perf_probes [i];";
  s.op->newline() << "rc = _stp_perf_init(stp, 0);";
  s.op->newline() << "if (rc) {";
  s.op->newline(1) << "probe_point = stp->probe->pp;";
  s.op->newline() << "for (j=0; j<i; j++) {";
  s.op->newline(1) << "_stp_perf_del(& stap_perf_probes [j]);";
  s.op->newline(-1) << "}"; // for unwind loop
  s.op->newline() << "break;";
  s.op->newline(-1) << "}"; // if-error
  s.op->newline() << "rc = stap_register_task_finder_target(&stp->tgt);";
  s.op->newline(-1) << "}"; // for loop
}


void
perf_derived_probe_group::emit_module_exit (systemtap_session& s)
{
  if (probes.empty()) return;

  s.op->newline() << "for (i=0; i<" << probes.size() << "; i++) {";
  s.op->newline(1) << "_stp_perf_del(& stap_perf_probes [i]);";
  s.op->newline(-1) << "}"; // for loop
}


struct perf_builder: public derived_probe_builder
{
    virtual void build(systemtap_session & sess,
                       probe * base, probe_point * location,
                       literal_map_t const & parameters,
                       vector<derived_probe *> & finished_results);

    static void register_patterns(systemtap_session& s);
};


void
perf_builder::build(systemtap_session & sess,
    probe * base,
    probe_point * location,
    literal_map_t const & parameters,
    vector<derived_probe *> & finished_results)
{
  // XXX need additional version checks too?
  // --- perhaps look for export of perf_event_create_kernel_counter
  if (sess.kernel_exports.find("perf_event_create_kernel_counter") == sess.kernel_exports.end())
    throw semantic_error (_("perf probes not available without exported perf_event_create_kernel_counter"));
  if (sess.kernel_config["CONFIG_PERF_EVENTS"] != "y")
    throw semantic_error (_("perf probes not available without CONFIG_PERF_EVENTS"));

  int64_t type;
  bool has_type = get_param(parameters, TOK_TYPE, type);
  assert(has_type);

  int64_t config;
  bool has_config = get_param(parameters, TOK_CONFIG, config);
  assert(has_config);

  int64_t period;
  bool has_period = get_param(parameters, TOK_SAMPLE, period);
  if (!has_period)
    period = 1000000; // XXX: better parametrize this default
  else if (period < 1)
    throw semantic_error(_("invalid perf sample period ") + lex_cast(period),
                         parameters.find(TOK_SAMPLE)->second->tok);
  bool proc_p;
  string proc_n;
  proc_p = has_null_param(parameters, TOK_PROCESS)
    || get_param(parameters, TOK_PROCESS, proc_n);

  if (sess.verbose > 1)
    clog << _F("perf probe type=%" PRId64 " config=%" PRId64 " period=%" PRId64, type, config, period) << endl;

  finished_results.push_back
    (new perf_derived_probe(base, location, type, config, period, proc_p, proc_n));
}


void
register_tapset_perf(systemtap_session& s)
{
  // NB: at this point, the binding is *not* unprivileged.

  derived_probe_builder *builder = new perf_builder();
  match_node* perf = s.pattern_root->bind(TOK_PERF);

  match_node* event = perf->bind_num(TOK_TYPE)->bind_num(TOK_CONFIG);
  event->bind(builder);
  event->bind_num(TOK_SAMPLE)->bind(builder);
  event->bind_str(TOK_PROCESS)->bind(builder);
  event->bind(TOK_PROCESS)->bind(builder);
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
