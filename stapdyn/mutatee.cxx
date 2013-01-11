// stapdyn mutatee functions
// Copyright (C) 2012-2013 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "mutatee.h"

extern "C" {
#include <signal.h>
#include <sys/types.h>
}

#include <BPatch_function.h>
#include <BPatch_image.h>
#include <BPatch_module.h>
#include <BPatch_point.h>
#include <BPatch_thread.h>

#include "dynutil.h"
#include "../util.h"

#include "../runtime/dyninst/stapdyn.h"

using namespace std;


// Create snippets for all the DWARF registers,
// in their architecture-specific order.
static void
get_dwarf_registers(BPatch_process *app,
                    vector<BPatch_snippet*>& registers)
{
#if defined(__i386__)
  static const char* const names[] = {
      "eax", "ecx", "edx", "ebx",
      "esp", "ebp", "esi", "edi",
      NULL };
#elif defined(__x86_64__)
  static const char* const names[] = {
      "rax", "rdx", "rcx", "rbx",
      "rsi", "rdi", "rbp", "rsp",
      "r8",  "r9",  "r10", "r11",
      "r12", "r13", "r14", "r15",
      NULL };
#else
  static const char* const names[] = { NULL };
#endif

  // First push the original PC, before instrumentation mucked anything up.
  // (There's also BPatch_actualAddressExpr for the instrumented result...)
  registers.push_back(new BPatch_originalAddressExpr());

  vector<BPatch_register> bpregs;
  app->getRegisters(bpregs);

  ostream& debug_reg = staplog(4);
  debug_reg << "pid " << app->getPid() << " has "
             << bpregs.size() << " registers available:";
  for (size_t i = 0; i < bpregs.size(); ++i)
    debug_reg << " " << bpregs[i].name();
  debug_reg << endl;

  // Look for each DWARF register in BPatch's register set.
  // O(m*n) loop, but neither array is very large
  for (const char* const* name = names; *name; ++name)
    {
      size_t i;
      for (i = 0; i < bpregs.size(); ++i)
        if (bpregs[i].name() == *name)
          {
            // Found it, add a snippet.
            registers.push_back(new BPatch_registerExpr(bpregs[i]));
            break;
          }

      // If we didn't find it, put a zero in its place.
      if (i >= bpregs.size())
        registers.push_back(new BPatch_constExpr((unsigned long)0));
    }
}


mutatee::mutatee(BPatch* patch, BPatch_process* process):
  pid(process? process->getPid() : 0),
  patch(patch), process(process), stap_dso(NULL),
  utrace_enter_function(NULL)
{
  get_dwarf_registers(process, registers);
}

mutatee::~mutatee()
{
  remove_instrumentation();
  unload_stap_dso();
  if (process)
    process->detach(true);
}


// Inject the stap module into the target process
bool
mutatee::load_stap_dso(const string& filename)
{
  stap_dso = process->loadLibrary(filename.c_str());
  if (!stap_dso)
    {
      staperror() << "Couldn't load " << filename
                  << " into the target process" << endl;
      return false;
    }
  return true;
}

// Unload the stap module from the target process
void
mutatee::unload_stap_dso()
{
  if (!process || !stap_dso)
    return;

  // XXX Dyninst has no unloadLibrary() yet, as of 8.0
  stap_dso = NULL;
}


// Remember utrace probes. They get handled when the associated
// callback hits.
void
mutatee::instrument_utrace_dynprobe(const dynprobe_location& probe)
{
  if (!stap_dso)
    return;

  if (utrace_enter_function == NULL)
    {
      vector<BPatch_function *> functions;
      stap_dso->findFunction("enter_dyninst_utrace_probe",
			     functions);
      if (!functions.empty())
	utrace_enter_function = functions[0];
    }

  // Remember this probe. It will get called from a callback function.
  attached_probes.push_back(probe);
}


// Handle "global" targets. They aren't really global, but non-path
// based probes, like:
//	probe process.begin { ... }
//	probe process(PID).begin { ... }
void
mutatee::instrument_global_dynprobe_target(const dynprobe_target& target)
{
  staplog(1) << "found global target, inserting "
             << target.probes.size() << " probes" << endl;

  for (size_t j = 0; j < target.probes.size(); ++j)
    {
      const dynprobe_location& probe = target.probes[j];

      // We already know this isn't a path-based probe. We've got 2
      // other qualifications here:
      // (1) Make sure this is a utrace probe by checking the flags.
      // (2) If PID was specified, does the pid match?
      if (((probe.flags & (STAPDYN_PROBE_FLAG_PROC_BEGIN
			   | STAPDYN_PROBE_FLAG_PROC_END
			   | STAPDYN_PROBE_FLAG_THREAD_BEGIN
			   | STAPDYN_PROBE_FLAG_THREAD_END)) != 0)
	  && (probe.offset == 0 || (int)probe.offset == process->getPid()))
	instrument_utrace_dynprobe(probe);
    }
}


// Given a target and the matching object, instrument all of the probes
// with calls to the stap_dso's entry function.
void
mutatee::instrument_dynprobe_target(BPatch_object* object,
                                    const dynprobe_target& target)
{
  if (!process || !stap_dso || !object)
    return;

  vector<BPatch_function *> functions;
  BPatch_function* enter_function = NULL;
  bool use_pt_regs = false;

  // XXX Until we know how to build pt_regs from here, we'll try the entry
  // function for individual registers first.
  if (!registers.empty())
    stap_dso->findFunction("enter_dyninst_uprobe_regs", functions);
  if (!functions.empty())
    enter_function = functions[0];

  // If the other entry wasn't found, or we don't have registers for it anyway,
  // try the form that takes pt_regs* and we'll just pass NULL.
  if (!enter_function)
    {
      stap_dso->findFunction("enter_dyninst_uprobe", functions);
      if (functions.empty())
        return;
      use_pt_regs = true;
      enter_function = functions[0];
    }

  staplog(1) << "found target \"" << target.path << "\", inserting "
             << target.probes.size() << " probes" << endl;

  for (size_t j = 0; j < target.probes.size(); ++j)
    {
      const dynprobe_location& probe = target.probes[j];

      if ((probe.flags & (STAPDYN_PROBE_FLAG_PROC_BEGIN
			  | STAPDYN_PROBE_FLAG_PROC_END
			  | STAPDYN_PROBE_FLAG_THREAD_BEGIN
			  | STAPDYN_PROBE_FLAG_THREAD_END)) != 0)
        {
	  instrument_utrace_dynprobe(probe);
	  continue;
	}

      // Convert the file offset to a memory address.
      Dyninst::Address address = object->fileOffsetToAddr(probe.offset);
      if (address == BPatch_object::E_OUT_OF_BOUNDS)
        {
          stapwarn() << "couldn't convert " << target.path << "+"
                     << lex_cast_hex(probe.offset) << " to an address" << endl;
          continue;
        }

      // Turn the address into instrumentation points.
      // NB: There may be multiple results if Dyninst determined that multiple
      // concrete functions have overlapping ranges.  Rare, but possible.
      vector<BPatch_point*> points;
      object->findPoints(address, points);
      if (points.empty())
        {
          stapwarn() << "couldn't find an instrumentation point at "
                     << lex_cast_hex(address) << ", " << target.path
                     << "+" << lex_cast_hex(probe.offset) << endl;
          continue;
        }

      // TODO check each point->getFunction()->isInstrumentable()

      if (probe.return_p)
        {
          // Transform the address points into function exits
          vector<BPatch_point*> return_points;
          for (size_t i = 0; i < points.size(); ++i)
            {
              vector<BPatch_point*>* exits =
                points[i]->getFunction()->findPoint(BPatch_locExit);
              if (!exits || exits->empty())
                {
                  stapwarn() << "couldn't find a return point from "
                             << lex_cast_hex(address) << ", " << target.path
                             << "+" << lex_cast_hex(probe.offset) << endl;
                  continue;
                }
              return_points.insert(return_points.end(),
                                   exits->begin(), exits->end());
            }
          points.swap(return_points);
        }

      // The entry function needs the index of this particular probe, then
      // the registers in whatever form we chose above.
      vector<BPatch_snippet *> args;
      args.push_back(new BPatch_constExpr((int64_t)probe.index));
      if (use_pt_regs)
        args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
      else
        {
          args.push_back(new BPatch_constExpr((unsigned long)registers.size()));
          args.insert(args.end(), registers.begin(), registers.end());
        }
      BPatch_funcCallExpr call(*enter_function, args);

      // Finally write the instrumentation for the probe!
      BPatchSnippetHandle* handle = process->insertSnippet(call, points);
      if (handle)
        snippets.push_back(handle);

      // XXX write the semaphore too!
    }
}


// Look for "global" (non-path based) probes and handle them.
void
mutatee::instrument_global_dynprobes(const vector<dynprobe_target>& targets)
{
  if (!process || !stap_dso || targets.empty())
    return;

  // Look for global (non path-based probes), and remember them.
  for (size_t i = 0; i < targets.size(); ++i)
    {
      const dynprobe_target& target = targets[i];

      // Do the real work...
      if (target.path.empty())
	instrument_global_dynprobe_target(target);
    }
}


// Look for all matches between this object and the targets
// we want to probe, then do the instrumentation.
void
mutatee::instrument_object_dynprobes(BPatch_object* object,
                                     const vector<dynprobe_target>& targets)
{
  if (!process || !stap_dso || !object || targets.empty())
    return;

  // We want to map objects by their full path, but the pathName from
  // Dyninst might be relative, so fill it out.
  string path = resolve_path(object->pathName());
  staplog(2) << "found object " << path << endl;

  // Match the object to our targets, and instrument matches.
  process->beginInsertionSet();
  for (size_t i = 0; i < targets.size(); ++i)
    {
      const dynprobe_target& target = targets[i];

      // Do the real work...
      if (path == target.path)
        instrument_dynprobe_target(object, target);
    }
  process->finalizeInsertionSet(false);
}

// FIXME: We have a problem with STAPDYN_PROBE_FLAG_PROC_END
// (i.e. 'process.end' probes).
//
// If we use dyninst's registerExitCallback(), when that callback
// hits, we can't stop the process before it exits. So, we can't call
// oneTimeCode() as we've done for the thread callbacks. So, that
// doesn't work.
//
// When registerExitCallback() hits, we could just run the probe
// locally, but then the probe context wouldn't be correct.

void
mutatee::exit_callback(BPatch_thread *proc)
{
  for (size_t i = 0; i < attached_probes.size(); ++i)
    {
      const dynprobe_location& probe = attached_probes[i];
      if (probe.flags & STAPDYN_PROBE_FLAG_PROC_END)
        {
	  staplog(1) << "found end proc probe, index = " << probe.index << endl;
	  if (utrace_enter_function != NULL)
	    {
	      vector<BPatch_snippet *> args;
	      args.push_back(new BPatch_constExpr((int64_t)probe.index));
	      args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
	      staplog(1) << "calling function with " << args.size() << " args"
			 << endl;
	      BPatch_funcCallExpr call(*utrace_enter_function, args);
	      proc->getProcess()->oneTimeCode(call);
	    }
	  else
	    staplog(1) << "no utrace enter function!" << endl;
	}
    }
}


void
mutatee::thread_callback(BPatch_thread *thread, bool create_p)
{
  // If 'thread' is the main process, just return. We can't stop the
  // process before it terminates.
  if (thread->getLWP() == process->getPid())
    return;

  // thread->oneTimeCode() requires that the process (not just the
  // thread) be stopped. So, stop the process if needed.
  bool stopped = process->isStopped();
  if (!stopped && !stop_execution(true))
    {
      staplog(3) << "stopping process failed, stopped="
		 << process->isStopped() << ", terminated="
		 << process->isTerminated() << endl;
      return;
    }

  for (size_t i = 0; i < attached_probes.size(); ++i)
    {
      const dynprobe_location& probe = attached_probes[i];
      if ((create_p && probe.flags & STAPDYN_PROBE_FLAG_THREAD_BEGIN)
	  || (!create_p && probe.flags & STAPDYN_PROBE_FLAG_THREAD_END))
        {
	  staplog(1) << "found " << (create_p ? "begin" : "end")
		     << " thread probe, index = " << probe.index << endl;
	  if (utrace_enter_function != NULL)
	    {
	      vector<BPatch_snippet *> args;
	      args.push_back(new BPatch_constExpr((int64_t)probe.index));
	      args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
	      BPatch_funcCallExpr call(*utrace_enter_function, args);
	      thread->oneTimeCode(call);
	    }
	  else
	    staplog(1) << "no utrace enter function!" << endl;
	}
    }

  // Let the process continue (if it wasn't stopped when we started).
  if (!stopped)
    continue_execution();
}

// Look for probe matches in all objects.
void
mutatee::instrument_dynprobes(const vector<dynprobe_target>& targets)
{
  if (!process || !stap_dso || targets.empty())
    return;

  BPatch_image* image = process->getImage();
  if (!image)
    return;

  // Match non object/path specific probes.
  instrument_global_dynprobes(targets);

  // Read all of the objects in the process.
  vector<BPatch_object *> objects;
  image->getObjects(objects);
  for (size_t i = 0; i < objects.size(); ++i)
    instrument_object_dynprobes(objects[i], targets);
}


// Remove all BPatch snippets we've instrumented in the target
void
mutatee::remove_instrumentation()
{
  if (!process || snippets.empty())
    return;

  process->beginInsertionSet();
  for (size_t i = 0; i < snippets.size(); ++i)
    process->deleteSnippet(snippets[i]);
  process->finalizeInsertionSet(false);
  snippets.clear();

  unload_stap_dso();
}


// Look up a function by name in the target and invoke it without parameters.
void
mutatee::call_function(const string& name)
{
  if (!stap_dso)
    return;

  vector<BPatch_function *> functions;
  stap_dso->findFunction(name.c_str(), functions);

  // XXX Dyninst can return multiple results, but we're not really
  // expecting that... should we really call them all anyway?
  for (size_t i = 0; i < functions.size(); ++i)
    {
      vector<BPatch_snippet *> args;
      BPatch_funcCallExpr call(*functions[i], args);
      process->oneTimeCode(call);
    }
}


// Send a signal to the process.
int
mutatee::kill(int signal)
{
  return pid ? ::kill(pid, signal) : -2;
}


void
mutatee::continue_execution()
{
  if (is_stopped())
    process->continueExecution();
}


bool
mutatee::stop_execution(bool wait_p)
{
  if (process->isStopped())
    {
      // Process is already stopped, no need to do anything else.
      return true;
    }

  staplog(1) << "stopping process" << endl;
  if (! process->stopExecution())
    {
      staplog(1) << "stopExecution failed!" << endl;
      return false;
    }
  if (wait_p)
    {
      staplog(1) << "waiting on process to stop" << endl;
      while (!process->isStopped() && !process->isTerminated())
	patch->waitForStatusChange();
      if (! process->isStopped() || process->isTerminated())
        {
	  if (! process->isStopped())
	    staplog(1) << "couldn't stop proc!" << endl;
	  return false;
	}
    }
  return true;
}
/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
