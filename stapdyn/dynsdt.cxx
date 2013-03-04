// dyninst example program
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.


#include <cstdlib>
#include <string>
#include <vector>

extern "C" {
#define __STDC_LIMIT_MACROS
#define __STDC_CONSTANT_MACROS
#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <stdint.h>
#include <err.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <gelf.h>
#include <libelf.h>
}

#include <BPatch.h>
#include <BPatch_function.h>
#include <BPatch_object.h>
#include <BPatch_point.h>

#include "dynutil.h"
#include "../util.h"


using namespace std;

struct sdt_point {
    string provider;
    string name;
    vector<pair<int, string> > args;
    GElf_Addr pc_offset;
    GElf_Addr sem_offset;
};


static vector<pair<int, string> >
split_sdt_args(const string& args)
{
  vector<pair<int, string> > vargs;
  size_t size_idx = 0;
  size_t at_idx = args.find('@');
  while (at_idx != string::npos)
    {
      size_t size_idx2 = string::npos;
      size_t at_idx2 = args.find('@', at_idx + 1);
      if (at_idx2 != string::npos)
        {
          size_idx2 = args.rfind(' ', at_idx2);
          if (size_idx2 == string::npos || size_idx2 <= size_idx)
            size_idx2 = at_idx2 = string::npos;
          else
            ++size_idx2;
        }

      string size_str = args.substr(size_idx, at_idx - size_idx);
      int size = atoi(size_str.c_str());
      string arg = args.substr(at_idx + 1, size_idx2 - at_idx - 2);
      vargs.push_back(make_pair(size, arg));

      size_idx = size_idx2;
      at_idx = at_idx2;
    }
  return vargs;
}


static vector<sdt_point>
find_sdt(const string& file)
{
  int fd = -1;
  size_t shstrndx;
  GElf_Addr sdt_base_addr = 0;
  GElf_Off sdt_base_offset = 0;
  GElf_Addr sdt_probes_addr = 0;
  GElf_Off sdt_probes_offset = 0;
  GElf_Off sdt_probes_virt_offset = 0;
  Elf* elf = NULL;
  Elf_Scn* scn = NULL;
  vector<sdt_point> points;

  fd = open(file.c_str(), O_RDONLY);
  if (fd < 0)
    {
      warn("can't open %s", file.c_str());
      goto ret;
    }

  elf = elf_begin(fd, ELF_C_READ_MMAP, NULL);
  if (!elf)
    {
      warn("can't read elf %s", file.c_str());
      goto ret_fd;
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

      const char* sh_name = elf_strptr(elf, shstrndx, shdr.sh_name);
      if (sh_name && !strcmp(".stapsdt.base", sh_name))
        {
          sdt_base_addr = shdr.sh_addr;
          sdt_base_offset = shdr.sh_offset;
          warnx("SDT base addr:%#" PRIx64 " offset:%#" PRIx64 "", sdt_base_addr, sdt_base_offset);
        }
      if (sh_name && !strcmp(".probes", sh_name))
        {
          sdt_probes_addr = shdr.sh_addr;
          sdt_probes_offset = shdr.sh_offset;
          warnx("SDT probes addr:%#" PRIx64 " offset:%#" PRIx64 "", sdt_probes_addr, sdt_probes_offset);
        }
    }

  // Data is usually loaded in a separate segment from text, with padding
  // between, so this computes the extra padding to subtract from semaphores.
  if (sdt_probes_offset)
    sdt_probes_virt_offset = (sdt_probes_addr - sdt_probes_offset) -
      (sdt_base_addr - sdt_base_offset);

  scn = NULL;
  while ((scn = elf_nextscn(elf, scn)))
    {
      GElf_Shdr shdr;
      if (gelf_getshdr (scn, &shdr) == NULL)
	continue;
      if (shdr.sh_type != SHT_NOTE)
	continue;
      if (shdr.sh_flags & SHF_ALLOC)
	continue;

      Elf_Data *data = elf_getdata (scn, NULL);
      size_t next;
      GElf_Nhdr nhdr;
      size_t name_off;
      size_t desc_off;
      for (size_t offset = 0;
	   (next = gelf_getnote (data, offset, &nhdr, &name_off, &desc_off)) > 0;
	   offset = next)
	{
	  char* cdata = ((char*)data->d_buf);
	  if (strcmp(cdata + name_off, "stapsdt") || nhdr.n_type != 3)
	    continue;

	  union {
	      Elf64_Addr a64[3];
	      Elf32_Addr a32[3];
	  } buf;
	  Elf_Data dst =
	    {
	      &buf, ELF_T_ADDR, EV_CURRENT,
	      gelf_fsize (elf, ELF_T_ADDR, 3, EV_CURRENT), 0, 0
	    };
	  assert (dst.d_size <= sizeof buf);

	  if (nhdr.n_descsz < dst.d_size + 3)
	    continue;

	  Elf_Data src =
	    {
	      cdata + desc_off, ELF_T_ADDR, EV_CURRENT,
	      dst.d_size, 0, 0
	    };

	  if (gelf_xlatetom (elf, &dst, &src,
			     elf_getident (elf, NULL)[EI_DATA]) == NULL)
	    warnx ("gelf_xlatetom: %s", elf_errmsg (-1));

	  sdt_point p;

	  const char* provider = cdata + desc_off + dst.d_size;
	  p.provider = provider;

	  const char* name = provider + p.provider.size() + 1;
	  p.name = name;

	  const char* args = name + p.name.size() + 1;
	  if (args < cdata + desc_off + nhdr.n_descsz)
	    p.args = split_sdt_args(args);


          GElf_Addr base_ref;
	  if (gelf_getclass (elf) == ELFCLASS32)
            {
              p.pc_offset = buf.a32[0];
              base_ref = buf.a32[1];
              p.sem_offset = buf.a32[2];
            }
	  else
            {
              p.pc_offset = buf.a64[0];
              base_ref = buf.a64[1];
              p.sem_offset = buf.a64[2];
            }

          // Convert to its relative position in the file
          p.pc_offset += sdt_base_offset - base_ref;
          if (p.sem_offset)
            p.sem_offset += sdt_base_offset - base_ref - sdt_probes_virt_offset;

          stringstream joined_args;
          for (size_t i = 0; i < p.args.size(); ++i)
            {
              if (i > 0)
                joined_args << ", ";
              joined_args << p.args[i].first << "@\"" << p.args[i].second << "\"";
            }
	  warnx("SDT offset:%#" PRIx64 " semaphore:%#" PRIx64 " %s:%s(%s)",
		p.pc_offset, p.sem_offset,
                p.provider.c_str(), p.name.c_str(),
		joined_args.str().c_str());

	  points.push_back(p);
	}
    }

  elf_end(elf);
ret_fd:
  close(fd);
ret:
  return points;
}


static void
instrument_sdt(BPatch_process* process,
               BPatch_object* object,
               sdt_point& p)
{
  BPatch_image* image = process->getImage();

  Dyninst::Address address = object->fileOffsetToAddr(p.pc_offset);
  if (address == BPatch_object::E_OUT_OF_BOUNDS)
    {
      warnx("couldn't convert %s:%s at %#" PRIx64 " to an address",
            p.provider.c_str(), p.name.c_str(), p.pc_offset);
      return;
    }

  vector<BPatch_point*> points;
  object->findPoints(address, points);
  if (points.empty())
    {
      warnx("couldn't find %s:%s at %#" PRIx64 " -> %#lx",
            p.provider.c_str(), p.name.c_str(), p.pc_offset, address);
      return;
    }

  ostringstream format;
  format << "SDT %s:%s";
  BPatch_Vector<BPatch_register> regs;
  process->getRegisters(regs);
  for (size_t i = 0; i < regs.size(); ++i)
    if (regs[i].name()[2] == 'x')
      format << "  " << regs[i].name() << ":%#lx";
  format << "\n";

  vector<BPatch_snippet *> printfArgs;
  printfArgs.push_back(new BPatch_constExpr(format.str().c_str()));
  printfArgs.push_back(new BPatch_constExpr(p.provider.c_str()));
  printfArgs.push_back(new BPatch_constExpr(p.name.c_str()));
  for (size_t i = 0; i < regs.size(); ++i)
    if (regs[i].name()[2] == 'x')
      printfArgs.push_back(new BPatch_registerExpr(regs[i]));

  vector<BPatch_function *> printfFuncs;
  image->findFunction("printf", printfFuncs);
  BPatch_funcCallExpr printfCall(*(printfFuncs[0]), printfArgs);

  warnx("inserting %s:%s at %#" PRIx64 " -> %#lx [%zu]",
        p.provider.c_str(), p.name.c_str(), p.pc_offset, address, points.size());
  process->insertSnippet(printfCall, points);

  if (p.sem_offset)
    {
      Dyninst::Address sem_address = object->fileOffsetToAddr(p.sem_offset);
      if (sem_address == BPatch_object::E_OUT_OF_BOUNDS)
        warnx("couldn't convert %s:%s semaphore %#" PRIx64 " to an address",
              p.provider.c_str(), p.name.c_str(), p.sem_offset);
      else
        {
          warnx("incrementing semaphore for %s:%s at %#" PRIx64 " -> %#lx",
                p.provider.c_str(), p.name.c_str(), p.sem_offset, sem_address);

          BPatch_type *sem_type = image->findType("unsigned short");
          BPatch_variableExpr *semaphore =
            process->createVariable(sem_address, sem_type);

          BPatch_arithExpr addOne(BPatch_assign, *semaphore,
                                  BPatch_arithExpr(BPatch_plus, *semaphore,
                                                   BPatch_constExpr(1)));
          process->oneTimeCode(addOne);
        }
    }
}


static void
instrument_sdt_points(BPatch_process* process,
                      BPatch_object* object,
                      vector<sdt_point>& points)
{
  process->beginInsertionSet();
  for (size_t i = 0; i < points.size(); ++i)
    instrument_sdt(process, object, points[i]);
  process->finalizeInsertionSet(false);
}


int
main(int argc, const char* argv[])
{
  if (argc < 2)
    errx(1, "usage: %s PROGRAM [ARGS..]", argv[0]);

  warnx("got args:");
  for (int i = 1; i < argc; ++i)
    warnx("  [%i] %s", i - 1, argv[i]);

  if (!check_dyninst_rt())
    return 1;

  if (!check_dyninst_sebools())
    return 1;

  BPatch patch;

  warnx("creating the process");
  string fullpath = find_executable(argv[1]);
  BPatch_process *app = patch.processCreate(fullpath.c_str(), &argv[1]);
  BPatch_image *image = app->getImage();

  warnx("scanning the process for sdt with BPatch_object");
  vector<BPatch_object *> objects;
  image->getObjects(objects);
  for (size_t i = 0; i < objects.size(); ++i)
    {
      BPatch_object* obj = objects[i];
      string file = obj->pathName();
      warnx("    found mapped object %s", file.c_str());

      vector<sdt_point> sdt_points = find_sdt(file);
      if (sdt_points.empty())
        continue;

      warnx("inserting sdt instrumentation in %s", file.c_str());
      instrument_sdt_points(app, obj, sdt_points);
    }

  warnx("**** running the process");
  app->continueExecution();
  while (!app->isTerminated()) {
    patch.waitForStatusChange();
  }

  warnx("done!");
  return check_dyninst_exit(app) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
