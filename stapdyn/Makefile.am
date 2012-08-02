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

bin_PROGRAMS =

if HAVE_DYNINST

dyninst_LIBS = -ldwarf -lcommon -linstructionAPI -lparseAPI -lpatchAPI \
	       -lpcontrol -lstackwalk -lsymtabAPI -ldyninstAPI

bin_PROGRAMS += stapdyn dynsdt

stapdyn_SOURCES = stapdyn.cxx dynutil.cxx ../util.cxx
stapdyn_CXXFLAGS = $(AM_CXXFLAGS) @DYNINST_CXXFLAGS@
stapdyn_LDFLAGS = $(AM_LDFLAGS) @DYNINST_LDFLAGS@
stapdyn_LDADD = -lelf $(dyninst_LIBS)

dynsdt_SOURCES = dynsdt.cxx dynutil.cxx ../util.cxx
dynsdt_CXXFLAGS = $(AM_CXXFLAGS) @DYNINST_CXXFLAGS@
dynsdt_LDFLAGS = $(AM_LDFLAGS) @DYNINST_LDFLAGS@
dynsdt_LDADD = -lelf $(dyninst_LIBS)

endif # HAVE_DYNINST

BUILT_SOURCES =
CLEANFILES =

# Arrange for git_version.h to be regenerated at every "make".
# Code fragment is based upon RadeonHD.am.

# The stamp file which is never created ensures that git_version.h is updated
# before every build. Having git_version.h in foo_SOURCES ensures a recompile
# of foo-bar.c if it is newer than the foo-bar.o file. Using noinst_foo_SOURCES
# instead of foo_SOURCES prevents shipping git_version.h in dist tarballs,
# which may cause false GIT_FOO readings.
BUILT_SOURCES += git_version.stamp
CLEANFILES += git_version.h
GIT_VERSION_CMD = $(SHELL) $(top_srcdir)/git_version.sh
git_version.stamp:
	@if test -f "$(srcdir)/git_version.h"; then \
		if test -f "git_version.h"; then :; \
		else \
			cp "$(srcdir)/git_version.h" "git_version.h"; \
		fi; \
	fi
	$(GIT_VERSION_CMD) -k -s $(top_srcdir) -o git_version.h
	@if test -s "$(srcdir)/git_version.h"; then \
		if cmp "$(srcdir)/git_version.h" "git_version.h"; then :; \
		else \
			echo "Error: $(srcdir)/git_version.h and git_version.h differ."; \
			echo "       You probably want to remove the former."; \
			exit 1; \
		fi; \
	fi

git_version.h:
	$(srcdir)/git_version.sh -k --srcdir $(srcdir)/.. -o git_version.h
