// stapdyn mutatee functions
// Copyright (C) 2012 Red Hat Inc.
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

#include "dynutil.h"
#include "../util.h"


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


mutatee::mutatee(BPatch_process* process):
  pid(process? process->getPid() : 0),
  process(process), stap_dso(NULL)
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
  BPatch_module* stap_mod = process->loadLibrary(filename.c_str());
  if (!stap_mod)
    {
      staperror() << "Couldn't load " << filename
                  << " into the target process" << endl;
      return false;
    }
  stap_dso = stap_mod->getObject();
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

  staplog(1) << "found target " << target.path << ", inserting "
             << target.probes.size() << " probes" << endl;

  for (size_t j = 0; j < target.probes.size(); ++j)
    {
      const dynprobe_location& probe = target.probes[j];

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


// Look for probe matches in all objects.
void
mutatee::instrument_dynprobes(const vector<dynprobe_target>& targets)
{
  if (!process || !stap_dso || targets.empty())
    return;

  BPatch_image* image = process->getImage();
  if (!image)
    return;

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


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
