# Makefile.am --- automake input file for systemtap-re2c

AUTOMAKE_OPTIONS=dejagnu

clean-local:
	-rm -f tmp_regtest*

check_PROGRAMS =

check_PROGRAMS += regtest

# check_SCRIPTS = regcomp.base/regtest.in.0 regcomp.base/regtest.in.1

regtest_SOURCES = regcomp.cxx regtest.cxx
regtest_SOURCES += actions.cc code.cc re2c-dfa.cxx regtranslate.cc re2c-globals.cxx
regtest_SOURCES += util_STANDALONE.cxx translator_output_STANDALONE.cxx
regtest_CXXFLAGS = $(AM_CXXFLAGS) -DREGCOMP_STANDALONE

BUILT_SOURCES =

# Arrange for the top level git_version.h to be regenerated at every "make".
BUILT_SOURCES += git_version.stamp
git_version.stamp ../git_version.h:
	$(MAKE) -C .. $(notdir $@)

RUNTESTDEFAULTFLAGS = --tool regcomp --srcdir $$srcdir
