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
#include <BPatch_object.h>

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


struct stapdyn_uprobe_probe {
    uint64_t index, offset, semaphore;
    stapdyn_uprobe_probe(uint64_t index, uint64_t offset, uint64_t semaphore):
      index(index), offset(offset), semaphore(semaphore) {}
};


struct stapdyn_uprobe_target {
    string path;
    vector<stapdyn_uprobe_probe> probes;
    stapdyn_uprobe_target(const char* path): path(path) {}
};


template <typename T> void
set_dlsym(T*& pointer, void* handle, const char* symbol, bool required=true)
{
  const char* err = dlerror(); // clear previous errors
  pointer = reinterpret_cast<T*>(dlsym(handle, symbol));
  if (required)
    {
      if ((err = dlerror()))
        throw runtime_error(err);
      if (pointer == NULL)
        throw runtime_error(string(symbol) + " is NULL");
    }
}


static void __attribute__ ((noreturn))
usage (int rc)
{
  clog << "Usage: stapdyn -c CMD MODULE" << endl;
  exit (rc);
}

static void
call_inferior_function(BPatch_process *app, const string& name)
{
  BPatch_image *image = app->getImage();

  BPatch_Vector<BPatch_function *> functions;
  image->findFunction(name.c_str(), functions);

  for (size_t i = 0; i < functions.size(); ++i)
    {
      BPatch_Vector<BPatch_snippet *> args;
      BPatch_funcCallExpr call(*functions[i], args);
      app->oneTimeCode(call);
      app->continueExecution();
    }
}

static void
instrument_uprobes(BPatch_process *app,
                   const vector<stapdyn_uprobe_target>& targets)
{
  if (!app || targets.empty())
    return;

  BPatch_image* image = app->getImage();
  if (!image)
    return;

  map<string, BPatch_object*> file_objects;
  vector<BPatch_object *> objects;
  image->getObjects(objects);
  for (size_t i = 0; i < objects.size(); ++i)
    {
      string path = objects[i]->pathName();
      char resolved_path[PATH_MAX];
      if (realpath(path.c_str(), resolved_path))
        file_objects[resolved_path] = objects[i];
    }

  BPatch_Vector<BPatch_function *> functions;
  image->findFunction("enter_dyninst_uprobe", functions);
  if (functions.empty()) return;
  BPatch_function& enter_function = *functions[0];

  app->beginInsertionSet();
  for (uint64_t i = 0; i < targets.size(); ++i)
    {
      const stapdyn_uprobe_target& target = targets[i];

      BPatch_object* object = file_objects[target.path];
      if (!object)
        {
          clog << "no object named " << target.path << endl;
          continue;
        }
      clog << "found target " << target.path << ", inserting "
           << target.probes.size() << " probes" << endl;

      for (uint64_t j = 0; j < target.probes.size(); ++j)
        {
          const stapdyn_uprobe_probe& probe = target.probes[j];

          Dyninst::Address address = object->fileOffsetToAddr(probe.offset);
          if (address == BPatch_object::E_OUT_OF_BOUNDS)
            {
              clog << "couldn't convert " << lex_cast_hex(probe.offset)
                   << " in " << target.path << " to an address" << endl;
              continue;
            }

          vector<BPatch_point*> points;
          object->findPoints(address, points);
          if (points.empty())
            {
              clog << "couldn't find instrumentation point for address "
                   << lex_cast_hex(address) << " at offset "
                   << lex_cast_hex(probe.offset) << " in " << target.path << endl;
              continue;
            }

          BPatch_Vector<BPatch_snippet *> args;
          args.push_back(new BPatch_constExpr((int64_t)probe.index));
          args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
          BPatch_funcCallExpr call(enter_function, args);
          app->insertSnippet(call, points);

          // XXX write the semaphore too!
        }
    }
  app->finalizeInsertionSet(false);
}

static vector<stapdyn_uprobe_target>
find_uprobes(void* module)
{
  vector<stapdyn_uprobe_target> targets;

  typeof(&stp_dyninst_target_count) target_count = NULL;
  typeof(&stp_dyninst_target_path) target_path = NULL;

  typeof(&stp_dyninst_probe_count) probe_count = NULL;
  typeof(&stp_dyninst_probe_target) probe_target = NULL;
  typeof(&stp_dyninst_probe_offset) probe_offset = NULL;
  typeof(&stp_dyninst_probe_semaphore) probe_semaphore = NULL;

  set_dlsym(target_count, module, "stp_dyninst_target_count", false);
  if (target_count == NULL) // no uprobes
    return targets;

  try
    {
      // if target_count exists, the rest of these should too
      set_dlsym(target_path, module, "stp_dyninst_target_path");
      set_dlsym(probe_count, module, "stp_dyninst_probe_count");
      set_dlsym(probe_target, module, "stp_dyninst_probe_target");
      set_dlsym(probe_offset, module, "stp_dyninst_probe_offset");
      set_dlsym(probe_semaphore, module, "stp_dyninst_probe_semaphore");
    }
  catch (runtime_error& e)
    {
      clog << program_invocation_short_name
           << ": dlsym " << e.what() << endl;
      return targets;
    }

  const uint64_t ntargets = target_count();
  for (uint64_t i = 0; i < ntargets; ++i)
    targets.push_back(stapdyn_uprobe_target(target_path(i)));

  const uint64_t nprobes = probe_count();
  for (uint64_t i = 0; i < nprobes; ++i)
    {
      uint64_t ti = probe_target(i);
      stapdyn_uprobe_probe p(i, probe_offset(i), probe_semaphore(i));
      if (ti < ntargets)
        targets[ti].probes.push_back(p);
    }

#if 0 // verbose debug
  for (uint64_t i = 0; i < ntargets; ++i)
    {
      stapdyn_uprobe_target& t = targets[i];
      clog << "target " << t.path << " has "
        << t.probes.size() << " probes" << endl;
      for (uint64_t j = 0; j < t.probes.size(); ++j)
        clog << "  offset:" << (void*)t.probes[i].offset
          << " semaphore:" << t.probes[i].semaphore << endl;
    }
#endif

  return targets;
}

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
      clog << program_invocation_short_name
           << ": dlsym " << e.what() << endl;
      return 1;
    }

  int rc = session_init();
  if (rc)
    {
      clog << program_invocation_short_name
           << ": stp_dyninst_session_init returned " << rc << endl;
      return 1;
    }

  // XXX TODO we really ought to wait for a signal before exiting, or for a
  // script requested exit (e.g. from a timer probe, which doesn't exist yet...)

  session_exit();

  return 0;
}

int
main(int argc, char * const argv[])
{
  const char* command = NULL;
  const char* module = NULL;

  int opt;
  while ((opt = getopt (argc, argv, "c:V")) != -1)
    {
      switch (opt)
        {
        case 'c':
          command = optarg;
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

  (void)dlerror(); // clear previous errors
  void* dlmodule = dlopen(module, RTLD_NOW);
  if (!dlmodule)
    {
      clog << program_invocation_short_name
           << ": dlopen " << dlerror() << endl;
      return 1;
    }

  if (!command)
    return run_simple_module(dlmodule);

  if (!check_dyninst_rt())
    return 1;

  if (!check_dyninst_sebools())
    return 1;

  BPatch patch;

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
      clog << "wordexp: parsing error (" << rc << ")" << endl;
      exit(rc);
    }

  string fullpath = find_executable(child_argv[0]);
  BPatch_process *app = patch.processCreate(fullpath.c_str(), child_argv);

  app->loadLibrary(module);

  instrument_uprobes(app, find_uprobes(dlmodule));

  call_inferior_function(app, "stp_dyninst_session_init");

  app->continueExecution();
  while (!app->isTerminated())
    patch.waitForStatusChange();

  /* When we get process detaching (rather than just exiting), need:
   *   call_inferior_function(app, "stp_dyninst_session_exit");
   */

  return check_dyninst_exit(app) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
