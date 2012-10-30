// stapdyn mutator functions
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "mutator.h"

#include <algorithm>

extern "C" {
#include <dlfcn.h>
#include <wordexp.h>
}

#include "dynutil.h"
#include "../util.h"

#include "../runtime/dyninst/stapdyn.h"


using namespace std;


// NB: since Dyninst callbacks have no context, we have to demux it
// to every mutator we've created, tracked by this vector.
static vector<mutator*> g_mutators;

static void
g_dynamic_library_callback(BPatch_thread *thread,
                           BPatch_module *module,
                           bool load)
{
  for (size_t i = 0; i < g_mutators.size(); ++i)
    g_mutators[i]->dynamic_library_callback(thread, module, load);
}


mutator:: mutator (const string& module_name)
  : module(NULL), module_name(resolve_path(module_name))
{
  // NB: dlopen does a library-path search if the filename doesn't have any
  // path components, which is why we use resolve_path(module_name)

  g_mutators.push_back(this);
}

mutator::~mutator ()
{
  mutatees.clear();

  if (module)
    {
      dlclose(module);
      module = NULL;
    }

  g_mutators.erase(find(g_mutators.begin(), g_mutators.end(), this));
}

// Load the stap module and initialize all probe info.
bool
mutator::load ()
{
  int rc;

  // Open the module directly, so we can query probes or run simple ones.
  (void)dlerror(); // clear previous errors
  module = dlopen(module_name.c_str(), RTLD_NOW);
  if (!module)
    {
      staperror() << "dlopen " << dlerror() << endl;
      return false;
    }

  if ((rc = find_dynprobes(module, targets)))
    return rc;
  if (!targets.empty())
    patch.registerDynLibraryCallback(g_dynamic_library_callback);

  return true;
}

// Create a new process with the given command line
bool
mutator::create_process(const string& command)
{
  // Split the command into words.  If wordexp can't do it,
  // we'll just run via "sh -c" instead.
  const char** child_argv;
  const char* sh_argv[] = { "/bin/sh", "-c", command.c_str(), NULL };
  wordexp_t words;
  int rc = wordexp (command.c_str(), &words, WRDE_NOCMD|WRDE_UNDEF);
  if (rc == 0)
    child_argv = (/*cheater*/ const char**) words.we_wordv;
  else if (rc == WRDE_BADCHAR)
    child_argv = sh_argv;
  else
    {
      staperror() << "wordexp parsing error (" << rc << ")" << endl;
      return false;
    }

  // Search the PATH if necessary, then create the target process!
  string fullpath = find_executable(child_argv[0]);
  BPatch_process* app = patch.processCreate(fullpath.c_str(), child_argv);
  if (!app)
    {
      staperror() << "Couldn't create the target process" << endl;
      return false;
    }

  boost::shared_ptr<mutatee> m(new mutatee(app));
  mutatees.push_back(m);

  if (!m->load_stap_dso(module_name))
    return false;

  if (!targets.empty())
    m->instrument_dynprobes(targets);

  return true;
}

// When there's no target command given,
// just run the begin/end basics directly.
bool
mutator::run_simple()
{
  if (!module)
    return false;

  typeof(&stp_dyninst_session_init) session_init = NULL;
  typeof(&stp_dyninst_session_exit) session_exit = NULL;
  try
    {
      set_dlsym(session_init, module, "stp_dyninst_session_init");
      set_dlsym(session_exit, module, "stp_dyninst_session_exit");
    }
  catch (runtime_error& e)
    {
      staperror() << e.what() << endl;
      return false;
    }

  int rc = session_init();
  if (rc)
    {
      stapwarn() << "stp_dyninst_session_init returned " << rc << endl;
      return false;
    }

  // XXX TODO we really ought to wait for a signal before exiting, or for a
  // script requested exit (e.g. from a timer probe, which doesn't exist yet...)

  session_exit();

  return true;
}


// Start the actual systemtap session!
bool
mutator::run ()
{
  // With no command, just run it right away and quit.
  if (mutatees.empty())
    return run_simple();

  // XXX The following code only works for a single mutatee. (PR14706)
  assert(mutatees.size() == 1);
  // When we get multiple mutatees, we'll need to always do the basic
  // begin/end/timers locally as in run_simple(), and then loop over all
  // children for continuation and exit status.

  // Get the stap module ready...
  mutatees[0]->call_function("stp_dyninst_session_init");

  // And away we go!
  mutatees[0]->continue_execution();

  // XXX Dyninst's notification FD is currently broken, so we'll fall back to
  // the fully-blocking wait for now.
#if 0
  // mask signals while we're preparing to poll
  {
    stap_sigmasker masked;

    // Polling with a notification FD lets us wait on Dyninst while still
    // letting signals break us out of the loop.
    while (!mutatees[0]->is_terminated())
      {
        pollfd pfd = { .fd=patch.getNotificationFD(),
                       .events=POLLIN, .revents=0 };

        int rc = ppoll (&pfd, 1, NULL, &masked.old);
        if (rc < 0 && errno != EINTR)
          break;

        // Acknowledge and activate whatever events are waiting
        patch.pollForStatusChange();
      }
  }
#else
  while (!mutatees[0]->is_terminated())
    patch.waitForStatusChange();
#endif

  // When we get process detaching (rather than just exiting), need:
  // mutatees[0]->call_function("stp_dyninst_session_exit");

  return mutatees[0]->check_exit();
}


// Callback to respond to dynamically loaded libraries.
// Check if it matches our targets, and instrument accordingly.
void
mutator::dynamic_library_callback(BPatch_thread *thread,
                                  BPatch_module *module,
                                  bool load)
{
  if (!load || !thread || !module)
    return;

  BPatch_process* process = thread->getProcess();

  for (size_t i = 0; i < mutatees.size(); ++i)
    if (*mutatees[i] == process)
      mutatees[i]->instrument_object_dynprobes(module->getObject(), targets);
}


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
