// translator output without the whole translation pass thing
// Copyright (C) 2005-2012 Red Hat Inc.
// Copyright (C) 2005-2008 Intel Corporation.
// Copyright (C) 2010 Novell Corporation.
//
// This file is part of systemtap, and is free software.  You can
// redistribute it and/or modify it under the terms of the GNU General
// Public License (GPL); either version 2, or (at your option) any
// later version.

#include "../translate.h"
#include <iostream>
#include <string>

using namespace std;

translator_output::translator_output (ostream& f):
  buf(0), o2 (0), o (f), tablevel (0)
{
}


translator_output::translator_output (const string& filename, size_t bufsize):
  buf (new char[bufsize]),
  o2 (new ofstream (filename.c_str ())),
  o (*o2),
  tablevel (0),
  filename (filename)
{
  o2->rdbuf()->pubsetbuf(buf, bufsize);
}


translator_output::~translator_output ()
{
  delete o2;
  delete [] buf;
}


ostream&
translator_output::newline (int indent)
{
  if (!  (indent > 0 || tablevel >= (unsigned)-indent)) o.flush ();
  assert (indent > 0 || tablevel >= (unsigned)-indent);

  tablevel += indent;
  o << "\n";
  for (unsigned i=0; i<tablevel; i++)
    o << "  ";
  return o;
}


void
translator_output::indent (int indent)
{
  if (!  (indent > 0 || tablevel >= (unsigned)-indent)) o.flush ();
  assert (indent > 0 || tablevel >= (unsigned)-indent);
  tablevel += indent;
}


ostream&
translator_output::line ()
{
  return o;
}

