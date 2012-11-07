// Copyright (C) Andrew Tridgell 2002 (original file)
// Copyright (C) 2006-2011 Red Hat Inc. (systemtap changes)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 2 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "../util.h"
#include <string>

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
}

using namespace std;

/* just the bare minimum required for regtest */

std::string autosprintf(const char* format, ...)
{
  va_list args;
  char *str;
  va_start (args, format);
  int rc = vasprintf (&str, format, args);
  if (rc < 0)
    {
      va_end(args);
      return _F("autosprintf/vasprintf error %d", rc);
    }
  string s = str;
  va_end (args);
  free (str);
  return s; /* by copy */
}
