#include <iostream>
#include <map>
#include <string>
#include <vector>

extern "C" {
#include <fcntl.h>
#include <getopt.h>
#include <gelf.h>
#include <libelf.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>
}

#include <BPatch.h>
#include <BPatch_object.h>

#include "config.h"
#include "git_version.h"

#include "dynutil.h"


#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)
#define DYNINST_FULL_VERSION \
  ( STRINGIFY(DYNINST_MAJOR) "." \
    STRINGIFY(DYNINST_MINOR) "." \
    STRINGIFY(DYNINST_SUBMINOR) )


using namespace std;

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
exit_hook(BPatch_thread *thread, BPatch_exitType)
{
  call_inferior_function(thread->getProcess(), "stp_dummy_exit");
}

// XXX: copied from runtime/dyninst/uprobes-dyninst.c
struct stapdu_target {
	char filename[240];
	uint64_t offset; /* the probe offset within the file */
	uint64_t sdt_sem_offset; /* the semaphore offset from process->base */
};

static void
instrument_uprobes(BPatch_process *app, Elf_Data* data)
{
  if (!app || !data) return;

  BPatch_image* image = app->getImage();
  if (!image) return;

  const stapdu_target* targets = static_cast<const stapdu_target*>(data->d_buf);
  size_t ntargets = data->d_size / sizeof(stapdu_target);
  if (!ntargets) return;

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
  for (size_t i = 0; i < ntargets; ++i)
    {
      const stapdu_target* target = &targets[i];
      clog << "found target " << target->filename
           << " 0x" << hex << target->offset << dec << endl;

      BPatch_object* object = file_objects[target->filename];
      if (!object)
        {
          clog << "no object named " << target->filename << endl;
          continue;
        }

      Dyninst::Address address = object->fileOffsetToAddr(target->offset);
      if (address == BPatch_object::E_OUT_OF_BOUNDS)
        {
          clog << "couldn't convert 0x" << hex << target->offset << dec
               << " in " << target->filename << " to an address" << endl;
          continue;
        }

      vector<BPatch_point*> points;
      object->findPoints(address, points);
      if (points.empty())
        {
          clog << "couldn't find instrumentation point 0x" << hex << target->offset << dec
               << " in " << target->filename << endl;
          continue;
        }

      BPatch_Vector<BPatch_snippet *> args;
      args.push_back(new BPatch_constExpr((int64_t)i)); // probe index
      args.push_back(new BPatch_constExpr((void*)NULL)); // pt_regs
      BPatch_funcCallExpr call(enter_function, args);
      app->insertSnippet(call, points);

      // XXX write the semaphore too!
    }
  app->finalizeInsertionSet(false);
}

static void
find_uprobes(BPatch_process *app, const string& module)
{
  int fd = -1;
  size_t shstrndx;
  Elf* elf = NULL;
  Elf_Scn* scn = NULL;

  fd = open(module.c_str(), O_RDONLY);
  if (fd < 0)
    {
      clog << "can't open " << module << endl;
      goto out;
    }

  elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
  if (!elf)
    {
      clog << "can't read elf " << module << endl;
      goto out_fd;
    }

  elf_getshdrstrndx (elf, &shstrndx);

  scn = NULL;
  while ((scn = elf_nextscn(elf, scn)))
    {
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) == NULL)
	continue;
      if (shdr.sh_type != SHT_PROGBITS)
	continue;
      if (!(shdr.sh_flags & SHF_ALLOC))
	continue;
      if (!strcmp(".stap_dyninst", elf_strptr(elf, shstrndx, shdr.sh_name)))
        instrument_uprobes(app, elf_rawdata(scn, NULL));
    }

  elf_end(elf);
out_fd:
  close(fd);
out:
  ;
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
          fprintf(stderr, "Systemtap Dyninst loader/runner (version %s/%s %s%s%s)\n"
                          "Copyright (C) 2012 Red Hat, Inc. and others\n"
                          "This is free software; see the source for copying conditions.\n",
                  VERSION, DYNINST_FULL_VERSION, GIT_MESSAGE,
                  (STAP_EXTRA_VERSION[0]?", ":""),
                  (STAP_EXTRA_VERSION[0]?STAP_EXTRA_VERSION:""));
          return 0;

        default:
          usage (1);
        }
    }

  if (optind == argc - 1)
    module = argv[optind];

  if (!module || !command)
    usage (1);

  if (!check_dyninst_rt())
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

  BPatch_process *app = patch.processCreate(child_argv[0], child_argv);

  app->loadLibrary(module);

  find_uprobes(app, module);

  call_inferior_function(app, "stp_dummy_init");
  patch.registerExitCallback(exit_hook);

  app->continueExecution();
  while (!app->isTerminated())
    patch.waitForStatusChange();

  return 0;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
