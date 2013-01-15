# Makefile.am --- automake input file for systemtap-re2c

AUTOMAKE_OPTIONS=dejagnu

clean-local:
	-rm -f tmp_regtest*

check_PROGRAMS =

check_PROGRAMS += regtest

# check_SCRIPTS = regcomp.base/regtest.in.0 regcomp.base/regtest.in.1

regtest_SOURCES = regcomp.cxx regtest.cxx
regtest_SOURCES += re2c-regex.cxx re2c-emit.cxx re2c-dfa.cxx re2c-globals.cxx
regtest_CXXFLAGS = $(AM_CXXFLAGS) -DREGCOMP_STANDALONE

BUILT_SOURCES =

# Arrange for the top level git_version.h to be regenerated at every "make".
BUILT_SOURCES += git_version.stamp
git_version.stamp ../git_version.h:
	$(MAKE) -C .. $(notdir $@)

RUNTESTDEFAULTFLAGS = --tool regcomp --srcdir $$srcdir
