# Makefile.am --- automake input file for systemtap-re2c

noinst_PROGRAMS =

noinst_PROGRAMS += regtest

regtest_SOURCES = regcomp.cxx regtest.cxx
regtest_SOURCES += actions.cc code.cc dfa.cc substr.cc translate.cc globals.cc
regtest_SOURCES += util_STANDALONE.cxx translator_output_STANDALONE.cxx
regtest_CXXFLAGS = $(AM_CXXFLAGS) -DREGCOMP_STANDALONE

BUILT_SOURCES =

# Arrange for the top level git_version.h to be regenerated at every "make".
BUILT_SOURCES += git_version.stamp
git_version.stamp ../git_version.h:
	$(MAKE) -C .. $(notdir $@)

