// stapdyn main program
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include <iostream>
#include <memory>

extern "C" {
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <wordexp.h>
}

#include "config.h"
#include "../git_version.h"
#include "../version.h"

#include "mutator.h"
#include "dynutil.h"


#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)
#define DYNINST_FULL_VERSION \
  ( STRINGIFY(DYNINST_MAJOR) "." \
    STRINGIFY(DYNINST_MINOR) "." \
    STRINGIFY(DYNINST_SUBMINOR) )


using namespace std;


// Print the obligatory usage message.
static void __attribute__ ((noreturn))
usage (int rc)
{
  clog << "Usage: " << program_invocation_short_name
       << " MODULE [-c CMD | -x PID]" << endl;
  exit (rc);
}


// This is main, the heart and soul of stapdyn, oh yeah!
int
main(int argc, char * const argv[])
{
  pid_t pid = 0;
  const char* command = NULL;
  const char* module = NULL;

  // First, option parsing.
  int opt;
  while ((opt = getopt (argc, argv, "c:x:vwV")) != -1)
    {
      switch (opt)
        {
        case 'c':
          command = optarg;
          break;

        case 'x':
          pid = atoi(optarg);
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
  if (optind == argc - 1)
    module = argv[optind];

  if (!module || (command && pid))
    usage (1);

  // Make sure that environment variables and selinux are set ok.
  if (!check_dyninst_rt())
    return 1;
  if (!check_dyninst_sebools())
    return 1;

  auto_ptr<mutator> session(new mutator(module));
  if (!session.get() || !session->load())
    {
      staperror() << "failed to create the mutator!" << endl;
      return 1;
    }

  if (command && !session->create_process(command))
    return 1;

  if (pid && !session->attach_process(pid))
    return 1;

  return session->run() ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
