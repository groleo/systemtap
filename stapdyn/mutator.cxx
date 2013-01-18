// stapdyn mutator functions
// Copyright (C) 2012-2013 Red Hat Inc.
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
#include <signal.h>
}

#include <BPatch_snippet.h>

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


static void
g_post_fork_callback(BPatch_thread *parent, BPatch_thread *child)
{
  for (size_t i = 0; i < g_mutators.size(); ++i)
    g_mutators[i]->post_fork_callback(parent, child);
}


static void
g_exit_callback(BPatch_thread *thread, BPatch_exitType type)
{
  for (size_t i = 0; i < g_mutators.size(); ++i)
    g_mutators[i]->exit_callback(thread, type);
}


static void
g_thread_create_callback(BPatch_process *proc, BPatch_thread *thread)
{
  for (size_t i = 0; i < g_mutators.size(); ++i)
    g_mutators[i]->thread_create_callback(proc, thread);
}


static void
g_thread_destroy_callback(BPatch_process *proc, BPatch_thread *thread)
{
  for (size_t i = 0; i < g_mutators.size(); ++i)
    g_mutators[i]->thread_destroy_callback(proc, thread);
}


static void
g_signal_handler(int signal)
{
  for (size_t i = 0; i < g_mutators.size(); ++i)
    g_mutators[i]->signal_callback(signal);
}


__attribute__((constructor))
static void
setup_signals (void)
{
  struct sigaction sa;
  static const int signals[] = {
      SIGHUP, SIGPIPE, SIGINT, SIGTERM,
  };

  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = g_signal_handler;
  sa.sa_flags = SA_RESTART;
  sigemptyset (&sa.sa_mask);
  for (size_t i = 0; i < sizeof(signals) / sizeof(*signals); ++i)
    sigaddset (&sa.sa_mask, signals[i]);
  for (size_t i = 0; i < sizeof(signals) / sizeof(*signals); ++i)
    sigaction (signals[i], &sa, NULL);
}


mutator:: mutator (const string& module_name):
  module(NULL), module_name(resolve_path(module_name)),
  p_target_created(false), signal_count(0)
{
  // NB: dlopen does a library-path search if the filename doesn't have any
  // path components, which is why we use resolve_path(module_name)

  g_mutators.push_back(this);
}

mutator::~mutator ()
{
  // Explicitly drop our mutatee references, so we better
  // control when their instrumentation is removed.
  target_mutatee.reset();
  mutatees.clear();

  if (module)
    {
      dlclose(module);
      module = NULL;
    }

  g_mutators.erase(find(g_mutators.begin(), g_mutators.end(), this));
}


// Do probes matching 'flag' exist?
bool
mutator::matching_probes_exist(uint64_t flag)
{
  for (size_t i = 0; i < targets.size(); ++i)
    {
      for (size_t j = 0; j < targets[i].probes.size(); ++j)
        {
	  if (targets[i].probes[j].flags & flag)
	    return true;
	}
    }
  return false;
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
    {
      patch.registerDynLibraryCallback(g_dynamic_library_callback);

      // Do we need a fork callback?
      if (matching_probes_exist(STAPDYN_PROBE_FLAG_PROC_BEGIN))
	patch.registerPostForkCallback(g_post_fork_callback);

      // Do we need a exit callback?
      if (matching_probes_exist(STAPDYN_PROBE_FLAG_PROC_END))
	patch.registerExitCallback(g_exit_callback);

      // Do we need a thread create callback?
      if (matching_probes_exist(STAPDYN_PROBE_FLAG_THREAD_BEGIN))
	patch.registerThreadEventCallback(BPatch_threadCreateEvent,
					  g_thread_create_callback);

      // Do we need a thread destroy callback?
      if (matching_probes_exist(STAPDYN_PROBE_FLAG_THREAD_END))
	patch.registerThreadEventCallback(BPatch_threadDestroyEvent,
					  g_thread_destroy_callback);
    }

  return true;
}

// Create a new process with the given command line
bool
mutator::create_process(const string& command)
{
  if (target_mutatee)
    {
      staperror() << "Already attached to a target process!" << endl;
      return false;
    }

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

  boost::shared_ptr<mutatee> m(new mutatee(&patch, app));
  mutatees.push_back(m);
  target_mutatee = m;
  p_target_created = true;

  if (!m->load_stap_dso(module_name))
    return false;

  if (!targets.empty())
    m->instrument_dynprobes(targets);

  return true;
}

// Attach to a specific existing process.
bool
mutator::attach_process(pid_t pid)
{
  if (target_mutatee)
    {
      staperror() << "Already attached to a target process!" << endl;
      return false;
    }

  BPatch_process* app = patch.processAttach(NULL, pid);
  if (!app)
    {
      staperror() << "Couldn't attach to the target process" << endl;
      return false;
    }

  boost::shared_ptr<mutatee> m(new mutatee(&patch, app));
  mutatees.push_back(m);
  target_mutatee = m;
  p_target_created = false;

  if (!m->load_stap_dso(module_name))
    return false;

  if (!targets.empty())
    m->instrument_dynprobes(targets);

  return true;
}

// Initialize the module session
bool
mutator::run_module_init()
{
  if (!module)
    return false;

  // First see if this is a shared-memory, multiprocess-capable module
  typeof(&stp_dyninst_shm_init) shm_init = NULL;
  typeof(&stp_dyninst_shm_connect) shm_connect = NULL;
  set_dlsym(shm_init, module, "stp_dyninst_shm_init", false);
  set_dlsym(shm_connect, module, "stp_dyninst_shm_connect", false);
  if (shm_init && shm_connect)
    {
      // Initialize the shared-memory locally.
      const char* shmem = shm_init();
      if (shmem == NULL)
        {
          stapwarn() << "stp_dyninst_shm_init failed!" << endl;
          return false;
        }
      module_shmem = shmem;
      // After the session is initilized, then we'll map shmem in the target
    }
  else if (target_mutatee)
    {
      // For modules that don't support shared-memory, but still have a target
      // process, we'll run init/exit in the target.
      target_mutatee->call_function("stp_dyninst_session_init");
      return true;
    }

  // From here, either this is a shared-memory module,
  // or we have no target and thus run init directly anyway.

  typeof(&stp_dyninst_session_init) session_init = NULL;
  try
    {
      set_dlsym(session_init, module, "stp_dyninst_session_init");
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

  // Now we map the shared-memory into the target
  if (target_mutatee && !module_shmem.empty())
    {
      vector<BPatch_snippet *> args;
      args.push_back(new BPatch_constExpr(module_shmem.c_str()));
      target_mutatee->call_function("stp_dyninst_shm_connect", args);
    }

  return true;
}

// Shutdown the module session
bool
mutator::run_module_exit()
{
  if (!module)
    return false;

  if (target_mutatee && module_shmem.empty())
    {
      // For modules that don't support shared-memory, but still have a target
      // process, we'll run init/exit in the target.
      // XXX This may already have been done in its deconstructor if the process exited.
      target_mutatee->call_function("stp_dyninst_session_exit");
      return true;
    }

  // From here, either this is a shared-memory module,
  // or we have no target and thus run exit directly anyway.

  typeof(&stp_dyninst_session_exit) session_exit = NULL;
  try
    {
      set_dlsym(session_exit, module, "stp_dyninst_session_exit");
    }
  catch (runtime_error& e)
    {
      staperror() << e.what() << endl;
      return false;
    }

  session_exit();
  return true;
}


// Check the status of all mutatees
bool
mutator::update_mutatees()
{
  if (signal_count >= (p_target_created ? 2 : 1))
    return false;

  if (target_mutatee && target_mutatee->is_terminated())
    return false;

  for (size_t i = 0; i < mutatees.size();)
    {
      boost::shared_ptr<mutatee> m = mutatees[i];
      if (m != target_mutatee && m->is_terminated())
        {
          mutatees.erase(mutatees.begin() + i);
          continue; // NB: without ++i
        }

      if (m->is_stopped())
        m->continue_execution();

      ++i;
    }

  return true;
}


// Start the actual systemtap session!
bool
mutator::run ()
{

  // Get the stap module ready...
  run_module_init();

  // And away we go!
  if (target_mutatee)
    {
      // For our first event, fire the target's process.begin probes (if any)
      target_mutatee->begin_callback();

      // XXX Dyninst's notification FD is currently broken, so we'll fall back
      // to the fully-blocking wait for now.
#if 0
      // mask signals while we're preparing to poll
      stap_sigmasker masked;

      // Polling with a notification FD lets us wait on Dyninst while still
      // letting signals break us out of the loop.
      while (update_mutatees())
        {
          pollfd pfd = { .fd=patch.getNotificationFD(),
                         .events=POLLIN, .revents=0 };

          int rc = ppoll (&pfd, 1, NULL, &masked.old);
          if (rc < 0 && errno != EINTR)
            break;

          // Acknowledge and activate whatever events are waiting
          patch.pollForStatusChange();
        }
#else
      while (update_mutatees())
        patch.waitForStatusChange();
#endif
    }
  else // !target_mutatee
    {
      // XXX TODO we really ought to wait for a signal before exiting,
      // or for a script requested exit (e.g. from a timer probe).
    }

  // Shutdown the stap module.
  run_module_exit();

  return target_mutatee ? target_mutatee->check_exit() : true;
}


// Find a mutatee which matches the given process, else return NULL
boost::shared_ptr<mutatee>
mutator::find_mutatee(BPatch_process* process)
{
  for (size_t i = 0; i < mutatees.size(); ++i)
    if (*mutatees[i] == process)
      return mutatees[i];
  return boost::shared_ptr<mutatee>();
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
  boost::shared_ptr<mutatee> mut = find_mutatee(process);
  if (mut)
    mut->instrument_object_dynprobes(module->getObject(), targets);
}


// Callback to respond to post fork events.  Check if it matches our
// targets, and handle accordingly.
void
mutator::post_fork_callback(BPatch_thread *parent, BPatch_thread *child)
{
  if (!child || !parent)
    return;

  BPatch_process* child_process = child->getProcess();
  BPatch_process* parent_process = parent->getProcess();

  staplog(1) << "post fork, parent " << parent_process->getPid()
	     << ", child " << child_process->getPid() << endl;

  boost::shared_ptr<mutatee> mut = find_mutatee(parent_process);
  if (mut)
    {
      // Clone the mutatee for the new process.
      boost::shared_ptr<mutatee> m(new mutatee(&patch, child_process));
      mutatees.push_back(m);
      m->copy_forked_instrumentation(*mut);

      // Trigger any process.begin probes.
      m->begin_callback();
    }
}


void
mutator::exit_callback(BPatch_thread *thread,
		       BPatch_exitType type __attribute__((unused)))
{
  if (!thread)
    return;

  // 'thread' is the thread that requested the exit, not necessarily the
  // main thread.
  BPatch_process* process = thread->getProcess();

  staplog(1) << "exit callback, pid = " << process->getPid() << endl;

  boost::shared_ptr<mutatee> mut = find_mutatee(process);
  if (mut)
    mut->exit_callback(thread);
}


void
mutator::thread_create_callback(BPatch_process *proc, BPatch_thread *thread)
{
  if (!proc || !thread)
    return;

  boost::shared_ptr<mutatee> mut = find_mutatee(proc);
  if (mut)
    mut->thread_callback(thread, true);
}


void
mutator::thread_destroy_callback(BPatch_process *proc, BPatch_thread *thread)
{
  if (!proc || !thread)
    return;

  boost::shared_ptr<mutatee> mut = find_mutatee(proc);
  if (mut)
    mut->thread_callback(thread, false);
}


// Callback to respond to signals.
void
mutator::signal_callback(int signal __attribute__((unused)))
{
  ++signal_count;

  // First time, try to kill the target process, only if we created it.
  if (signal_count == 1 && target_mutatee && p_target_created)
    target_mutatee->kill(SIGTERM);

  // Second time, mutator::run should break out anyway
  if (signal_count == 2)
    stapwarn() << "Multiple interrupts received, exiting..." << endl;

  // Third time's the charm; the user wants OUT!
  if (signal_count >= 3)
    {
      staperror() << "Too many interrupts received, aborting now!" << endl;
      _exit (1);
    }
}


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
