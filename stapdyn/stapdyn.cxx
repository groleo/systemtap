#include <iostream>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>
}

#include <BPatch.h>
#include <BPatch_function.h>
#include <BPatch_image.h>
#include <BPatch_object.h>
#include <BPatch_point.h>
#include <BPatch_process.h>
#include <BPatch_snippet.h>

#include "config.h"
#include "../git_version.h"
#include "../version.h"

#include "dynutil.h"
#include "../util.h"

#include "../runtime/dyninst/stapdyn.h"

#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)
#define DYNINST_FULL_VERSION \
  ( STRINGIFY(DYNINST_MAJOR) "." \
    STRINGIFY(DYNINST_MINOR) "." \
    STRINGIFY(DYNINST_SUBMINOR) )


using namespace std;


// The individual probes' info read from the stap module.
struct stapdyn_uprobe_probe {
    uint64_t index;     // The index as counted by the module.
    uint64_t offset;    // The file offset of the probe's address.
    uint64_t semaphore; // The file offset of the probe's semaphore.
    stapdyn_uprobe_probe(uint64_t index, uint64_t offset, uint64_t semaphore):
      index(index), offset(offset), semaphore(semaphore) {}
};


// The probe target info read from the stap module.
struct stapdyn_uprobe_target {
    string path; // The fully resolved path to the file.
    vector<stapdyn_uprobe_probe> probes; // All probes in this target.
    stapdyn_uprobe_target(const char* path): path(path) {}
};


// We will have to use at least some globals like these, because all of the
// Dyninst are anonymous (i.e. no user data pointer).  We should probably
// bundle them into a session object when I'm less lazy...

static BPatch_object *g_stap_dso = NULL;        // the module that we have loaded
static BPatch_process * g_child_process = NULL; // the process we started
static vector<stapdyn_uprobe_target> g_targets; // the probe targets in our module


//
// Logging, warnings, and errors, oh my!
//

// A null-sink output stream, similar to /dev/null
// (no buffer -> badbit -> quietly suppressed output)
static ostream nullstream(NULL);

// verbosity, increased by -v
static unsigned stapdyn_log_level = 0;

// Whether to suppress warnings, set by -w
static bool stapdyn_supress_warnings = false;

// Return a stream for logging at the given verbosity level.
static ostream&
staplog(unsigned level=0)
{
  if (level > stapdyn_log_level)
    return nullstream;
  return clog << program_invocation_short_name << ": ";
}

// Return a stream for warning messages.
static ostream&
stapwarn(void)
{
  if (stapdyn_supress_warnings)
    return nullstream;
  return clog << program_invocation_short_name << ": WARNING: ";
}

// Return a stream for error messages.
static ostream&
staperror(void)
{
  return clog << program_invocation_short_name << ": ERROR: ";
}


// Set a typed function pointer by looking it up in a dlopened module.
// If flagged as 'required', throw an exception if missing or NULL.
template <typename T> void
set_dlsym(T*& pointer, void* handle, const char* symbol, bool required=true)
{
  const char* err = dlerror(); // clear previous errors
  pointer = reinterpret_cast<T*>(dlsym(handle, symbol));
  if (required)
    {
      if ((err = dlerror()))
        throw runtime_error("dlsym " + string(err));
      if (pointer == NULL)
        throw runtime_error("dlsym " + string(symbol) + " is NULL");
    }
}


// Print the obligatory usage message.
static void __attribute__ ((noreturn))
usage (int rc)
{
  clog << "Usage: " << program_invocation_short_name
       << " MODULE -c CMD" << endl;
  exit (rc);
}


// Look up a function by name in the target and invoke it without parameters.
static void
call_inferior_function(BPatch_process *app,
                       BPatch_object *object,
                       const string& name)
{
  vector<BPatch_function *> functions;
  object->findFunction(name.c_str(), functions);

  // XXX Dyninst can return multiple results, but we're not really
  // expecting that... should we really call them all anyway?
  for (size_t i = 0; i < functions.size(); ++i)
    {
      vector<BPatch_snippet *> args;
      BPatch_funcCallExpr call(*functions[i], args);
      app->oneTimeCode(call);
    }
}


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

  BPatch_Vector<BPatch_register> bpregs;
  app->getRegisters(bpregs);

  // Look for each DWARF register in BPatch's register set.
  // O(m*n) loop, but neither array is very large
  for (const char* const* name = names; *name; ++name)
    {
      // XXX Dyninst is currently limited in how many individual function
      // arguments it can pass, so we'll have to cut this short...
      if (registers.size() > 8) break;

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


// Given a target and the matchin object, instrument all of the target probes
// with calls to the stap_dso's entry function.
static void
instrument_uprobe_target(BPatch_process* app,
                         BPatch_object* stap_dso,
                         BPatch_object* object,
                         const stapdyn_uprobe_target& target)
{
  if (!app || !stap_dso || !object)
    return;

  BPatch_function* enter_function = NULL;
  vector<BPatch_function *> functions;
  vector<BPatch_snippet *> enter_args;

  // XXX Until we know how to build pt_regs from here, we'll try the entry
  // function for individual registers first.
  stap_dso->findFunction("enter_dyninst_uprobe_regs", functions);
  if (!functions.empty())
    {
      get_dwarf_registers(app, enter_args);
      if (enter_args.empty())
        enter_function = functions[0];
    }

  // If the other entry wasn't found, or we don't have registers for it anyway,
  // try the form that takes pt_regs* and we'll just pass NULL.
  if (!enter_function)
    {
      stap_dso->findFunction("enter_dyninst_uprobe", functions);
      if (functions.empty())
        return;
      enter_function = functions[0];
    }

  staplog(1) << "found target " << target.path << ", inserting "
             << target.probes.size() << " probes" << endl;

  for (size_t j = 0; j < target.probes.size(); ++j)
    {
      const stapdyn_uprobe_probe& probe = target.probes[j];

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

      // The entry function needs the index of this particular probe, then
      // the registers in whatever form we chose above.
      vector<BPatch_snippet *> args;
      args.push_back(new BPatch_constExpr((int64_t)probe.index));
      if (enter_args.empty())
        args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
      else
        {
          args.push_back(new BPatch_constExpr((unsigned long)enter_args.size()));
          args.insert(args.end(), enter_args.begin(), enter_args.end());
        }
      BPatch_funcCallExpr call(*enter_function, args);

      // Finally write the instrumentation for the probe!
      app->insertSnippet(call, points);

      // XXX write the semaphore too!
    }
}


// Look for all matches between objects in the process and targets
// we want to probe, then do the instrumentation.
static void
instrument_uprobes(BPatch_process *app, BPatch_object* stap_dso,
                   const vector<stapdyn_uprobe_target>& targets)
{
  if (!app || !stap_dso || targets.empty())
    return;

  BPatch_image* image = app->getImage();
  if (!image)
    return;

  // Read all of the objects in the process.
  map<string, BPatch_object*> file_objects;
  vector<BPatch_object *> objects;
  image->getObjects(objects);
  for (size_t i = 0; i < objects.size(); ++i)
    {
      // We want to map objects by their full path, but the pathName from
      // Dyninst might be relative, so use realpath().
      string path = objects[i]->pathName();
      char resolved_path[PATH_MAX];
      if (realpath(path.c_str(), resolved_path))
        {
          staplog(2) << "found object " << resolved_path << endl;
          file_objects[resolved_path] = objects[i];
        }
    }

  // Match objects to our targets, and instrument matches.
  app->beginInsertionSet();
  for (size_t i = 0; i < targets.size(); ++i)
    {
      const stapdyn_uprobe_target& target = targets[i];

      BPatch_object* object = file_objects[target.path];
      if (!object)
        {
          // If we didn't find it now, it might be dlopen'ed later.
          // It may even be an object in a child process, or never seen
          // at all for that matter.
          staplog(2) << "no initial object named " << target.path << endl;
          continue;
        }

      // Do the real work...
      instrument_uprobe_target(app, stap_dso, object, target);
    }
  app->finalizeInsertionSet(false);
}


// Callback to respond to dynamically loaded libraries.
// Check if it matches our targets, and instrument accordingly.
static void
dynamic_library_callback(BPatch_thread *thread,
                         BPatch_module *module,
                         bool load)
{
  if (!load || !g_stap_dso || !thread || !module)
    return;

  BPatch_process* app = thread->getProcess();
  if (!app || app != g_child_process)
    return;

  BPatch_object* object = module->getObject();
  if (!object)
    return;

  // Compare by full path, so relative differences don't throw us off.
  string path = object->pathName();
  char resolved_path[PATH_MAX];
  if (!realpath(path.c_str(), resolved_path))
    return;

  staplog(2) << "found dynamic object " << resolved_path << endl;

  // Search all the targets for matches, and instrument them.
  app->beginInsertionSet();
  for (size_t i = 0; i < g_targets.size(); ++i)
    {
      const stapdyn_uprobe_target& target = g_targets[i];
      if (target.path == resolved_path)
        instrument_uprobe_target(app, g_stap_dso, object, target);
    }
  app->finalizeInsertionSet(false);
}


// Look for "uprobes" in the stap module which need Dyninst instrumentation.
static int
find_uprobes(void* module, vector<stapdyn_uprobe_target>& targets)
{
  // We query for probes with a function interface, so first we have
  // to get function pointers from the stap module.
  typeof(&stp_dyninst_target_count) target_count = NULL;
  typeof(&stp_dyninst_target_path) target_path = NULL;

  typeof(&stp_dyninst_probe_count) probe_count = NULL;
  typeof(&stp_dyninst_probe_target) probe_target = NULL;
  typeof(&stp_dyninst_probe_offset) probe_offset = NULL;
  typeof(&stp_dyninst_probe_semaphore) probe_semaphore = NULL;

  // If we don't even have this, then there aren't any uprobes in the module.
  set_dlsym(target_count, module, "stp_dyninst_target_count", false);
  if (target_count == NULL)
    return 0;

  try
    {
      // If target_count exists, the rest of these should too.
      set_dlsym(target_path, module, "stp_dyninst_target_path");
      set_dlsym(probe_count, module, "stp_dyninst_probe_count");
      set_dlsym(probe_target, module, "stp_dyninst_probe_target");
      set_dlsym(probe_offset, module, "stp_dyninst_probe_offset");
      set_dlsym(probe_semaphore, module, "stp_dyninst_probe_semaphore");
    }
  catch (runtime_error& e)
    {
      staperror() << e.what() << endl;
      return 1;
    }

  // Construct all the targets in the module.
  const uint64_t ntargets = target_count();
  for (uint64_t i = 0; i < ntargets; ++i)
    targets.push_back(stapdyn_uprobe_target(target_path(i)));

  // Construct all the probes in the module,
  // and associate each with their target.
  const uint64_t nprobes = probe_count();
  for (uint64_t i = 0; i < nprobes; ++i)
    {
      uint64_t ti = probe_target(i);
      stapdyn_uprobe_probe p(i, probe_offset(i), probe_semaphore(i));
      if (ti < ntargets)
        targets[ti].probes.push_back(p);
    }

  // For debugging, dump what we found.
  for (uint64_t i = 0; i < ntargets; ++i)
    {
      stapdyn_uprobe_target& t = targets[i];
      staplog(3) << "target " << t.path << " has "
                 << t.probes.size() << " probes" << endl;
      for (uint64_t j = 0; j < t.probes.size(); ++j)
        staplog(3) << "  offset:" << (void*)t.probes[i].offset
                   << " semaphore:" << t.probes[i].semaphore << endl;
    }

  return 0;
}


// When there's no target command given,
// just run the begin/end basics directly.
static int
run_simple_module(void* module)
{
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
      return 1;
    }

  int rc = session_init();
  if (rc)
    {
      stapwarn() << "stp_dyninst_session_init returned " << rc << endl;
      return 1;
    }

  // XXX TODO we really ought to wait for a signal before exiting, or for a
  // script requested exit (e.g. from a timer probe, which doesn't exist yet...)

  session_exit();

  return 0;
}


// This is main, the heart and soul of stapdyn, oh yeah!
int
main(int argc, char * const argv[])
{
  const char* command = NULL;
  const char* module = NULL;

  // First, option parsing.
  int opt;
  while ((opt = getopt (argc, argv, "c:vwV")) != -1)
    {
      switch (opt)
        {
        case 'c':
          command = optarg;
          break;

        case 'v':
          ++stapdyn_log_level;
          break;

        case 'w':
          stapdyn_supress_warnings = true;
          break;

        case 'V':
          fprintf(stderr, "Systemtap Dyninst loader/runner (version %s/%s, %s)\n"
                          "Copyright (C) 2012 Red Hat, Inc. and others\n"
                          "This is free software; see the source for copying conditions.\n",
                  VERSION, DYNINST_FULL_VERSION, STAP_EXTENDED_VERSION);
          return 0;

        default:
          usage (1);
        }
    }

  // The first non-option is our stap module, required.
  string module_str;
  if (optind == argc - 1)
    {
      module_str = argv[optind];

      // dlopen does a library-path search if the filename doesn't have any
      // path components.  We never want that, so give it a minimal ./ path.
      if (module_str.find('/') == string::npos)
        module_str.insert(0, "./");
      module = module_str.c_str();
    }

  if (!module)
    usage (1);

  // Open the module directly, so we can query probes or run simple ones.
  (void)dlerror(); // clear previous errors
  void* dlmodule = dlopen(module, RTLD_NOW);
  if (!dlmodule)
    {
      staperror() << "dlopen " << dlerror() << endl;
      return 1;
    }

  // With no command, just run it right away and quit.
  if (!command)
    return run_simple_module(dlmodule);

  // Make sure that environment variables and selinux are set ok.
  if (!check_dyninst_rt())
    return 1;
  if (!check_dyninst_sebools())
    return 1;

  BPatch patch;

  // Split the command into words.  If wordexp can't do it,
  // we'll just run via "sh -c" instead.
  const char** child_argv;
  const char* sh_argv[] = { "/bin/sh", "-c", command, NULL };
  wordexp_t words;
  int rc = wordexp (command, &words, WRDE_NOCMD|WRDE_UNDEF);
  if (rc == 0)
    child_argv = (/*cheater*/ const char**) words.we_wordv;
  else if (rc == WRDE_BADCHAR)
    child_argv = sh_argv;
  else
    {
      staperror() << "wordexp parsing error (" << rc << ")" << endl;
      return 1;
    }

  // Search the PATH if necessary, then create the target process!
  string fullpath = find_executable(child_argv[0]);
  BPatch_process* app = patch.processCreate(fullpath.c_str(), child_argv);
  if (!app)
    {
      staperror() << "Couldn't create the target process" << endl;
      return 1;
    }

  // Load the stap module into the target process.
  g_child_process = app;
  BPatch_module* stap_mod = app->loadLibrary(module);
  if (!app)
    {
      staperror() << "Couldn't load " << module
                  << " into the target process" << endl;
      return 1;
    }
  g_stap_dso = stap_mod->getObject();

  // Find and instrument uprobes in the target
  if ((rc = find_uprobes(dlmodule, g_targets)))
    return rc;
  if (!g_targets.empty())
    {
      patch.registerDynLibraryCallback(dynamic_library_callback);
      instrument_uprobes(app, g_stap_dso, g_targets);
    }

  // Get the stap module ready...
  call_inferior_function(app, g_stap_dso, "stp_dyninst_session_init");

  // And away we go!
  app->continueExecution();
  while (!app->isTerminated())
    patch.waitForStatusChange();

  /* When we get process detaching (rather than just exiting), need:
   *   call_inferior_function(app, g_stap_dso, "stp_dyninst_session_exit");
   */

  return check_dyninst_exit(app) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
