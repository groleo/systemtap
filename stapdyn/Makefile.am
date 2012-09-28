# Makefile.am --- automake input file for systemtap dyninst tools

AM_CFLAGS = -Wall -Werror -Wunused -W -Wformat=2 -Wno-format-nonliteral
AM_CXXFLAGS = -Wall -Werror -Wunused -W -Wformat=2 -Wno-format-nonliteral
AM_CPPFLAGS = -D_GNU_SOURCE
AM_CPPFLAGS += -I$(srcdir)/../includes
AM_CPPFLAGS += -I$(builddir)/../includes/sys
AM_CPPFLAGS += -DBINDIR='"$(bindir)"' -DSYSCONFDIR='"$(sysconfdir)"' -DPKGDATADIR='"${pkgdatadir}"' -DPKGLIBDIR='"$(pkglibexecdir)"' -DLOCALEDIR='"$(localedir)"'

AM_CFLAGS += @PIECFLAGS@
AM_CXXFLAGS += @PIECXXFLAGS@
AM_LDFLAGS = @PIELDFLAGS@

if HAVE_SELINUX
AM_CPPFLAGS += $(selinux_CFLAGS)
AM_LDFLAGS += $(selinux_LIBS)
endif

bin_PROGRAMS =
noinst_PROGRAMS =

if HAVE_DYNINST

dyninst_LIBS = -ldwarf -lcommon -linstructionAPI -lparseAPI -lpatchAPI \
	       -lpcontrol -lstackwalk -lsymtabAPI -ldyninstAPI

bin_PROGRAMS += stapdyn
noinst_PROGRAMS += dynsdt

stapdyn_SOURCES = stapdyn.cxx dynutil.cxx ../util.cxx
stapdyn_CXXFLAGS = $(AM_CXXFLAGS) @DYNINST_CXXFLAGS@
stapdyn_LDFLAGS = $(AM_LDFLAGS) @DYNINST_LDFLAGS@
stapdyn_LDADD = -ldl $(dyninst_LIBS)

dynsdt_SOURCES = dynsdt.cxx dynutil.cxx ../util.cxx
dynsdt_CXXFLAGS = $(AM_CXXFLAGS) @DYNINST_CXXFLAGS@
dynsdt_LDFLAGS = $(AM_LDFLAGS) @DYNINST_LDFLAGS@
dynsdt_LDADD = -lelf $(dyninst_LIBS)

endif # HAVE_DYNINST

BUILT_SOURCES =

# Arrange for the top-level git_version.h to be regenerated at every "make".
BUILT_SOURCES += git_version.stamp
git_version.stamp ../git_version.h:
	$(MAKE) -C .. $(notdir $@)
