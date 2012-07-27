#include <string>
#include <cstdlib>

extern "C" {
#include <err.h>
#include <link.h>
}

#include "dynutil.h"
#include "../util.h"

using namespace std;


// Callback for dl_iterate_phdr to look for libdyninstAPI.so
static int
guess_dyninst_rt_callback(struct dl_phdr_info *info,
                          size_t size __attribute__ ((unused)),
                          void *data)
{
  string& libdyninstAPI = *static_cast<string*>(data);

  const string name = info->dlpi_name ?: "(null)";
  if (name.find("libdyninstAPI.so") != string::npos)
    libdyninstAPI = name;

  return 0;
}

// Look for libdyninstAPI.so in our own process, and use that
// to guess the path for libdyninstAPI_RT.so
static const string
guess_dyninst_rt(void)
{
  string libdyninstAPI;
  dl_iterate_phdr(guess_dyninst_rt_callback, &libdyninstAPI);

  string libdyninstAPI_RT;
  size_t so = libdyninstAPI.rfind(".so");
  if (so != string::npos)
    {
      libdyninstAPI_RT = libdyninstAPI;
      libdyninstAPI_RT.insert(so, "_RT");
    }
  return libdyninstAPI_RT;
}

// Check that environment DYNINSTAPI_RT_LIB exists and is a valid file.
// If not, try to guess a good value and set it.
bool
check_dyninst_rt(void)
{
  static const char rt_env_name[] = "DYNINSTAPI_RT_LIB";
  const char* rt_env = getenv(rt_env_name);
  if (rt_env)
    {
      if (file_exists(rt_env))
        return true;
      warnx("Invalid %s: \"%s\"", rt_env_name, rt_env);
    }

  const string rt = guess_dyninst_rt();
  if (rt.empty() || !file_exists(rt))
    {
      warnx("Can't find libdyninstAPI_RT.so; try setting %s", rt_env_name);
      return false;
    }

  if (setenv(rt_env_name, rt.c_str(), 1) != 0)
    {
      warn("Can't set %s", rt_env_name);
      return false;
    }

  return true;
}


/* vim: set sw=2 ts=8 cino=>4,n-2,{2,^-2,t0,(0,u0,w1,M1 : */
