// -*- C++ -*-
// Copyright (C) 2012 Red Hat Inc.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.
//
// ... TODOXXX additional blurb for re2c ...

#include "regcomp.h"
#include "../translate.h"
#include <iostream>
#include <cstdio>
#include <cstdlib>

using namespace std;

void print_usage(char *progname)
{
  fprintf (stderr, "usage: one of the following\n");
  fprintf (stderr, "$ %s 0 <regex> <string>\n", progname);
  fprintf (stderr, "$ %s 1 <regex> <string>\n", progname);
  fprintf (stderr, "$ %s 2 <regex> <string>\n", progname);
}

int main(int argc, char *argv [])
{
  if (argc < 2)
    {
      print_usage (argv[0]);
      exit (1);
    }

  int test_type = atoi (argv[1]);
  switch (test_type)
    {
    case 0:
    case 1:
    case 2:
      // glibc-style test syntax
      {
        if (argc != 4) { print_usage (argv[0]); exit (1); }
        string s(argv[2]);
        stapdfa d("do_match", s);
        translator_output o(cout);

        string match_expr = "foo"; // TODOXXX escape argv[3]

        // emit code skeleton
        o.line() << "// test output for systemtap-re2c";
        o.newline() << "#include <stdio.h>";
        o.newline() << "#include <stdlib.h>";

        o.newline();
        d.emit_declaration (&o);
        o.newline();

        o.newline() << "int main()";
        o.newline() << "{";
        o.indent(1);
        o.newline() << "int ans = ";
        d.emit_matchop (&o, match_expr); // TODOXXX escape argv[3]
        o.line() << ";";
        o.newline() << "printf(\"match %s\\n\", ans ? \"succeed\" : \"fail\");";
        o.newline() << "exit(ans);";
        o.newline(-1) << "}";
        o.newline();

        break;
      }
    default:
      print_usage (argv[0]);
      exit (1);
    }
}
