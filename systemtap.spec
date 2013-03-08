%{!?with_sqlite: %global with_sqlite 1}
%{!?with_docs: %global with_docs 1}
%{!?with_crash: %global with_crash 1}
%{!?with_rpm: %global with_rpm 1}
%{!?with_bundled_elfutils: %global with_bundled_elfutils 0}
%{!?elfutils_version: %global elfutils_version 0.142}
%{!?pie_supported: %global pie_supported 1}
%{!?with_boost: %global with_boost 0}
%{!?with_publican: %global with_publican 1}
%if 0%{?rhel}
%{!?publican_brand: %global publican_brand RedHat}
%else
%{!?publican_brand: %global publican_brand fedora}
%endif
%ifnarch %{arm}
%{!?with_dyninst: %global with_dyninst 0%{?fedora} >= 18 || 0%{?rhel} >= 7}
%else
%{!?with_dyninst: %global with_dyninst 0}
%endif
%{!?with_systemd: %global with_systemd 0%{?fedora} >= 19}
%{!?with_emacsvim: %global with_emacsvim 1}

Name: systemtap
Version: 2.2
Release: 1%{?dist}
# for version, see also configure.ac


# Packaging abstract:
#
# systemtap              empty req:-client req:-devel
# systemtap-server       /usr/bin/stap-server*, req:-devel
# systemtap-devel        /usr/bin/stap, runtime, tapset, req:kernel-devel
# systemtap-runtime      /usr/bin/staprun, /usr/bin/stapsh, /usr/bin/stapdyn
# systemtap-client       /usr/bin/stap, samples, docs, tapset(bonus), req:-runtime
# systemtap-initscript   /etc/init.d/systemtap, req:systemtap
# systemtap-sdt-devel    /usr/include/sys/sdt.h /usr/bin/dtrace
# systemtap-testsuite    /usr/share/systemtap/testsuite*, req:systemtap, req:sdt-devel
#
# Typical scenarios:
#
# stap-client:           systemtap-client
# stap-server:           systemtap-server
# local user:            systemtap
#
# Unusual scenarios:
# 
# intermediary stap-client for --remote:       systemtap-client (-runtime unused)
# intermediary stap-server for --use-server:   systemtap-server (-devel unused)

Summary: Programmable system-wide instrumentation system
Group: Development/System
License: GPLv2+
URL: http://sourceware.org/systemtap/
Source: ftp://sourceware.org/pub/systemtap/releases/systemtap-%{version}.tar.gz

# Build*
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: gcc-c++
BuildRequires: gettext-devel
BuildRequires: nss-devel avahi-devel pkgconfig
%if %{with_dyninst}
BuildRequires: dyninst-devel >= 8.0
BuildRequires: libselinux-devel
%endif
%if %{with_sqlite}
BuildRequires: sqlite-devel
%endif
# Needed for libstd++ < 4.0, without <tr1/memory>
%if %{with_boost}
BuildRequires: boost-devel
%endif
%if %{with_crash}
BuildRequires: crash-devel zlib-devel
%endif
%if %{with_rpm}
BuildRequires: rpm-devel glibc-headers
%endif
%if %{with_bundled_elfutils}
Source1: elfutils-%{elfutils_version}.tar.gz
Patch1: elfutils-portability.patch
BuildRequires: m4
%global setup_elfutils -a1
%else
BuildRequires: elfutils-devel >= %{elfutils_version}
%endif
%if %{with_docs}
BuildRequires: /usr/bin/latex /usr/bin/dvips /usr/bin/ps2pdf latex2html
%if 0%{?fedora} >= 18 || 0%{?rhel} >= 7
BuildRequires: tex(fullpage.sty) tex(fancybox.sty) tex(bchr7t.tfm)
%endif
# On F10, xmlto's pdf support was broken off into a sub-package,
# called 'xmlto-tex'.  To avoid a specific F10 BuildReq, we'll do a
# file-based buildreq on '/usr/share/xmlto/format/fo/pdf'.
BuildRequires: xmlto /usr/share/xmlto/format/fo/pdf
%if %{with_publican}
BuildRequires: publican
BuildRequires: /usr/share/publican/Common_Content/%{publican_brand}/defaults.cfg
%endif
%endif
%if %{with_emacsvim}
BuildRequires: emacs
%endif

# Install requirements
Requires: systemtap-client = %{version}-%{release}
Requires: systemtap-devel = %{version}-%{release}

%description
SystemTap is an instrumentation system for systems running Linux.
Developers can write instrumentation scripts to collect data on
the operation of the system.  The base systemtap package contains/requires
the components needed to locally develop and execute systemtap scripts.

# ------------------------------------------------------------------------

%package server
Summary: Instrumentation System Server
Group: Development/System
License: GPLv2+
URL: http://sourceware.org/systemtap/
Requires: systemtap-devel = %{version}-%{release}
# On RHEL[45], /bin/mktemp comes from the 'mktemp' package.  On newer
# distributions, /bin/mktemp comes from the 'coreutils' package.  To
# avoid a specific RHEL[45] Requires, we'll do a file-based require.
Requires: nss /bin/mktemp
Requires: zip unzip
Requires(pre): shadow-utils
Requires(post): chkconfig
Requires(preun): chkconfig
Requires(preun): initscripts
Requires(postun): initscripts
BuildRequires: nss-devel avahi-devel

%description server
This is the remote script compilation server component of systemtap.
It announces itself to nearby clients with avahi (if available), and
compiles systemtap scripts to kernel objects on their demand.


%package devel
Summary: Programmable system-wide instrumentation system - development headers, tools
Group: Development/System
License: GPLv2+
URL: http://sourceware.org/systemtap/
Requires: kernel >= 2.6.9-11
# Alternate kernel packages kernel-PAE-devel et al. have a virtual
# provide for kernel-devel, so this requirement does the right thing,
# at least past RHEL4.
Requires: kernel-devel
Requires: gcc make
# Suggest: kernel-debuginfo

%description devel
This package contains the components needed to compile a systemtap
script from source form into executable (.ko) forms.  It may be
installed on a self-contained developer workstation (along with the
systemtap-client and systemtap-runtime packages), or on a dedicated
remote server (alongside the systemtap-server package).  It includes
a copy of the standard tapset library and the runtime library C files.


%package runtime
Summary: Programmable system-wide instrumentation system - runtime
Group: Development/System
License: GPLv2+
URL: http://sourceware.org/systemtap/
Requires: kernel >= 2.6.9-11
Requires(pre): shadow-utils

%description runtime
SystemTap runtime contains the components needed to execute
a systemtap script that was already compiled into a module
using a local or remote systemtap-devel installation.


%package client
Summary: Programmable system-wide instrumentation system - client
Group: Development/System
License: GPLv2+
URL: http://sourceware.org/systemtap/
Requires: zip unzip
Requires: systemtap-runtime = %{version}-%{release}
Requires: coreutils grep sed unzip zip
Requires: openssh-clients

%description client
This package contains/requires the components needed to develop 
systemtap scripts, and compile them using a local systemtap-devel 
or a remote systemtap-server installation, then run them using a
local or remote systemtap-runtime.  It includes script samples and
documentation, and a copy of the tapset library for reference.


%package initscript
Summary: Systemtap Initscripts
Group: Development/System
License: GPLv2+
URL: http://sourceware.org/systemtap/
Requires: systemtap = %{version}-%{release}
Requires(post): chkconfig
Requires(preun): chkconfig
Requires(preun): initscripts
Requires(postun): initscripts

%description initscript
Sysvinit scripts to launch selected systemtap scripts at system startup.


%package sdt-devel
Summary: Static probe support tools
Group: Development/System
License: GPLv2+ and Public Domain
URL: http://sourceware.org/systemtap/

%description sdt-devel
This package includes the <sys/sdt.h> header file used for static
instrumentation compiled into userspace programs and libraries, along
with the optional dtrace-compatibility preprocessor to process related
.d files into tracing-macro-laden .h headers.


%package testsuite
Summary: Instrumentation System Testsuite
Group: Development/System
License: GPLv2+
URL: http://sourceware.org/systemtap/
Requires: systemtap = %{version}-%{release}
Requires: systemtap-sdt-devel = %{version}-%{release}
Requires: systemtap-server = %{version}-%{release}
Requires: dejagnu which elfutils grep nc
Requires: gcc gcc-c++ make glibc-devel
%ifnarch ia64
Requires: prelink
%endif
# testsuite/systemtap.server/client.exp needs avahi
Requires: avahi
%if %{with_crash}
# testsuite/systemtap.base/crash.exp needs crash
Requires: crash
%endif
%ifarch x86_64
Requires: /usr/lib/libc.so
# ... and /usr/lib/libgcc_s.so.*
# ... and /usr/lib/libstdc++.so.*
%endif

%description testsuite
This package includes the dejagnu-based systemtap stress self-testing
suite.  This may be used by system administrators to thoroughly check
systemtap on the current system.


# ------------------------------------------------------------------------

%prep
%setup -q %{?setup_elfutils}

%if %{with_bundled_elfutils}
cd elfutils-%{elfutils_version}
%patch1 -p1
sleep 1
find . \( -name Makefile.in -o -name aclocal.m4 \) -print | xargs touch
sleep 1
find . \( -name configure -o -name config.h.in \) -print | xargs touch
cd ..
%endif

%build

%if %{with_bundled_elfutils}
# Build our own copy of elfutils.
%global elfutils_config --with-elfutils=elfutils-%{elfutils_version}

# We have to prevent the standard dependency generation from identifying
# our private elfutils libraries in our provides and requires.
%global _use_internal_dependency_generator	0
%global filter_eulibs() /bin/sh -c "%{1} | sed '/libelf/d;/libdw/d;/libebl/d'"
%global __find_provides %{filter_eulibs /usr/lib/rpm/find-provides}
%global __find_requires %{filter_eulibs /usr/lib/rpm/find-requires}

# This will be needed for running stap when not installed, for the test suite.
%global elfutils_mflags LD_LIBRARY_PATH=`pwd`/lib-elfutils
%endif

# Enable/disable the dyninst pure-userspace backend
%if %{with_dyninst}
%global dyninst_config --with-dyninst
%else
%global dyninst_config --without-dyninst
%endif

# Enable/disable the sqlite coverage testing support
%if %{with_sqlite}
%global sqlite_config --enable-sqlite
%else
%global sqlite_config --disable-sqlite
%endif

# Enable/disable the crash extension
%if %{with_crash}
%global crash_config --enable-crash
%else
%global crash_config --disable-crash
%endif

# Enable/disable the code to find and suggest needed rpms
%if %{with_rpm}
%global rpm_config --with-rpm
%else
%global rpm_config --without-rpm
%endif

%if %{with_docs}
%global docs_config --enable-docs
%else
%global docs_config --disable-docs
%endif

# Enable pie as configure defaults to disabling it
%if %{pie_supported}
%global pie_config --enable-pie
%else
%global pie_config --disable-pie
%endif

%if %{with_publican}
%global publican_config --enable-publican --with-publican-brand=%{publican_brand}
%else
%global publican_config --disable-publican
%endif


%configure %{?elfutils_config} %{dyninst_config} %{sqlite_config} %{crash_config} %{docs_config} %{pie_config} %{publican_config} %{rpm_config} --disable-silent-rules --with-extra-version="rpm %{version}-%{release}"
make %{?_smp_mflags}

%if %{with_emacsvim}
%{_emacs_bytecompile} emacs/systemtap-mode.el
%endif

%install
rm -rf ${RPM_BUILD_ROOT}
make DESTDIR=$RPM_BUILD_ROOT install
%find_lang %{name}

# We want the examples in the special doc dir, not the build install dir.
# We build it in place and then move it away so it doesn't get installed
# twice. rpm can specify itself where the (versioned) docs go with the
# %doc directive.
mv $RPM_BUILD_ROOT%{_datadir}/doc/systemtap/examples examples

# Fix paths in the example & testsuite scripts
find examples testsuite -type f -name '*.stp' -print0 | xargs -0 sed -i -r -e '1s@^#!.+stap@#!%{_bindir}/stap@'

# To make rpmlint happy, remove any .gitignore files in the testsuite.
find testsuite -type f -name '.gitignore' -print0 | xargs -0 rm -f

# Because "make install" may install staprun with whatever mode, the
# post-processing programs rpmbuild runs won't be able to read it.
# So, we change permissions so that they can read it.  We'll set the
# permissions back to 04110 in the %files section below.
chmod 755 $RPM_BUILD_ROOT%{_bindir}/staprun

#install the useful stap-prep script
install -c -m 755 stap-prep $RPM_BUILD_ROOT%{_bindir}/stap-prep

# Copy over the testsuite
cp -rp testsuite $RPM_BUILD_ROOT%{_datadir}/systemtap

%if %{with_docs}
# We want the manuals in the special doc dir, not the generic doc install dir.
# We build it in place and then move it away so it doesn't get installed
# twice. rpm can specify itself where the (versioned) docs go with the
# %doc directive.
mkdir docs.installed
mv $RPM_BUILD_ROOT%{_datadir}/doc/systemtap/*.pdf docs.installed/
mv $RPM_BUILD_ROOT%{_datadir}/doc/systemtap/tapsets docs.installed/
%if %{with_publican}
mv $RPM_BUILD_ROOT%{_datadir}/doc/systemtap/SystemTap_Beginners_Guide docs.installed/
%endif
%endif

mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/stap-server
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/lib/stap-server
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/log/stap-server
touch $RPM_BUILD_ROOT%{_localstatedir}/log/stap-server/log
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/cache/systemtap
mkdir -p $RPM_BUILD_ROOT%{_localstatedir}/run/systemtap
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d
install -m 644 initscript/logrotate.stap-server $RPM_BUILD_ROOT%{_sysconfdir}/logrotate.d/stap-server
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/rc.d/init.d/
install -m 755 initscript/systemtap $RPM_BUILD_ROOT%{_sysconfdir}/rc.d/init.d/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/systemtap
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/systemtap/conf.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/systemtap/script.d
install -m 644 initscript/config.systemtap $RPM_BUILD_ROOT%{_sysconfdir}/systemtap/config
%if %{with_systemd}
mkdir -p $RPM_BUILD_ROOT%{_unitdir}
touch $RPM_BUILD_ROOT%{_unitdir}/stap-server.service
install -m 644 stap-server.service $RPM_BUILD_ROOT%{_unitdir}/stap-server.service
mkdir -p $RPM_BUILD_ROOT/usr/lib/tmpfiles.d
install -m 644 stap-server.conf $RPM_BUILD_ROOT/usr/lib/tmpfiles.d/stap-server.conf
%else
install -m 755 initscript/stap-server $RPM_BUILD_ROOT%{_sysconfdir}/rc.d/init.d/
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/stap-server/conf.d
mkdir -p $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig
install -m 644 initscript/config.stap-server $RPM_BUILD_ROOT%{_sysconfdir}/sysconfig/stap-server
%endif

%if %{with_emacsvim}
mkdir -p $RPM_BUILD_ROOT%{_emacs_sitelispdir}
install -p -m 644 emacs/systemtap-mode.el* $RPM_BUILD_ROOT%{_emacs_sitelispdir}
mkdir -p $RPM_BUILD_ROOT%{_emacs_sitestartdir}
install -p -m 644 emacs/systemtap-init.el $RPM_BUILD_ROOT%{_emacs_sitestartdir}/systemtap-init.el
for subdir in ftdetect ftplugin indent syntax
do
    mkdir -p $RPM_BUILD_ROOT%{_datadir}/vim/vimfiles/$subdir
    install -p -m 644 vim/$subdir/*.vim $RPM_BUILD_ROOT%{_datadir}/vim/vimfiles/$subdir
done
%endif


%clean
rm -rf ${RPM_BUILD_ROOT}

%pre runtime
getent group stapusr >/dev/null || groupadd -g 156 -r stapusr 2>/dev/null || groupadd -r stapusr
getent group stapsys >/dev/null || groupadd -g 157 -r stapsys 2>/dev/null || groupadd -r stapsys
getent group stapdev >/dev/null || groupadd -g 158 -r stapdev 2>/dev/null || groupadd -r stapdev
exit 0

%pre server
getent group stap-server >/dev/null || groupadd -g 155 -r stap-server 2>/dev/null || groupadd -r stap-server
getent passwd stap-server >/dev/null || \
  useradd -c "Systemtap Compile Server" -u 155 -g stap-server -d %{_localstatedir}/lib/stap-server -r -s /sbin/nologin stap-server 2>/dev/null || \
  useradd -c "Systemtap Compile Server" -g stap-server -d %{_localstatedir}/lib/stap-server -r -s /sbin/nologin stap-server
test -e ~stap-server && chmod 755 ~stap-server

if [ ! -f ~stap-server/.systemtap/rc ]; then
  mkdir -p ~stap-server/.systemtap
  chown stap-server:stap-server ~stap-server/.systemtap
  echo "--rlimit-as=614400000 --rlimit-cpu=60 --rlimit-nproc=20 --rlimit-stack=1024000 --rlimit-fsize=51200000" > ~stap-server/.systemtap/rc
  chown stap-server:stap-server ~stap-server/.systemtap/rc
fi
exit 0

%post server
test -e %{_localstatedir}/log/stap-server/log || {
     touch %{_localstatedir}/log/stap-server/log
     chmod 664 %{_localstatedir}/log/stap-server/log
     chown stap-server:stap-server %{_localstatedir}/log/stap-server/log
}
# If it does not already exist, as stap-server, generate the certificate
# used for signing and for ssl.
if test ! -e ~stap-server/.systemtap/ssl/server/stap.cert; then
   runuser -s /bin/sh - stap-server -c %{_libexecdir}/systemtap/stap-gen-cert >/dev/null
fi
# Activate the service
%if %{with_systemd}
     /bin/systemctl enable stap-server.service >/dev/null 2>&1 || :
     /bin/systemd-tmpfiles --create >/dev/null 2>&1 || :
%else
    /sbin/chkconfig --add stap-server
%endif
exit 0

%triggerin client -- systemtap-server
if test -e ~stap-server/.systemtap/ssl/server/stap.cert; then
   # echo Authorizing ssl-peer/trusted-signer certificate for local systemtap-server
   %{_libexecdir}/systemtap/stap-authorize-cert ~stap-server/.systemtap/ssl/server/stap.cert %{_sysconfdir}/systemtap/ssl/client >/dev/null
   %{_libexecdir}/systemtap/stap-authorize-cert ~stap-server/.systemtap/ssl/server/stap.cert %{_sysconfdir}/systemtap/staprun >/dev/null
fi
exit 0
# XXX: corresponding %triggerun?

%preun server
# Check that this is the actual deinstallation of the package, as opposed to
# just removing the old package on upgrade.
if [ $1 = 0 ] ; then
    %if %{with_systemd}
       /bin/systemctl --no-reload disable stap-server.service >/dev/null 2>&1 || :
       /bin/systemctl stop stap-server.service >/dev/null 2>&1 || :
    %else
        /sbin/service stap-server stop >/dev/null 2>&1
    	/sbin/chkconfig --del stap-server
    %endif
fi
exit 0

%postun server
# Check whether this is an upgrade of the package.
# If so, restart the service if it's running
if [ "$1" -ge "1" ] ; then
    %if %{with_systemd}
    	/bin/systemctl restart stap-server.service >/dev/null 2>&1 || :
    %else
        /sbin/service stap-server condrestart >/dev/null 2>&1 || :
    %endif
fi
exit 0

%post initscript
%if %{with_systemd}
    /bin/systemctl enable stap-server.service >/dev/null 2>&1 || :
     /bin/systemd-tmpfiles --create >/dev/null 2>&1 || :
%else
    /sbin/chkconfig --add systemtap
%endif
exit 0

%preun initscript
# Check that this is the actual deinstallation of the package, as opposed to
# just removing the old package on upgrade.
if [ $1 = 0 ] ; then
    %if %{with_systemd}
    	/bin/systemctl --no-reload disable stap-server.service >/dev/null 2>&1 || :
	/bin/systemctl stop stap-server.service >/dev/null 2>&1 || :
    %else
        /sbin/service systemtap stop >/dev/null 2>&1
    	/sbin/chkconfig --del systemtap
    %endif
fi
exit 0

%postun initscript
# Check whether this is an upgrade of the package.
# If so, restart the service if it's running
if [ "$1" -ge "1" ] ; then
    %if %{with_systemd}
        /bin/systemctl restart stap-server.service >/dev/null 2>&1 || :
    %else
        /sbin/service systemtap condrestart >/dev/null 2>&1 || :
    %endif
fi
exit 0

%post
# Remove any previously-built uprobes.ko materials
(make -C %{_datadir}/systemtap/runtime/uprobes clean) >/dev/null 2>&1 || true
(/sbin/rmmod uprobes) >/dev/null 2>&1 || true

%preun
# Ditto
(make -C %{_datadir}/systemtap/runtime/uprobes clean) >/dev/null 2>&1 || true
(/sbin/rmmod uprobes) >/dev/null 2>&1 || true

# ------------------------------------------------------------------------

%files -f systemtap.lang
# The master "systemtap" rpm doesn't include any files.

%files server -f systemtap.lang
%defattr(-,root,root)
%{_bindir}/stap-server
%dir %{_libexecdir}/systemtap
%{_libexecdir}/systemtap/stap-serverd
%{_libexecdir}/systemtap/stap-start-server
%{_libexecdir}/systemtap/stap-stop-server
%{_libexecdir}/systemtap/stap-gen-cert
%{_libexecdir}/systemtap/stap-sign-module
%{_libexecdir}/systemtap/stap-authorize-cert
%{_libexecdir}/systemtap/stap-env
%{_mandir}/man7/error*
%{_mandir}/man7/stappaths.7*
%{_mandir}/man7/warning*
%{_mandir}/man8/stap-server.8*
%if %{with_systemd}
%{_unitdir}/stap-server.service
/usr/lib/tmpfiles.d/stap-server.conf
%else
%{_sysconfdir}/rc.d/init.d/stap-server
%dir %{_sysconfdir}/stap-server/conf.d
%config(noreplace) %{_sysconfdir}/sysconfig/stap-server
%endif
%config(noreplace) %{_sysconfdir}/logrotate.d/stap-server
%dir %{_sysconfdir}/stap-server
%dir %attr(0750,stap-server,stap-server) %{_localstatedir}/lib/stap-server
%dir %attr(0755,stap-server,stap-server) %{_localstatedir}/log/stap-server
%ghost %config(noreplace) %attr(0644,stap-server,stap-server) %{_localstatedir}/log/stap-server/log
%ghost %attr(0755,stap-server,stap-server) %{_localstatedir}/run/stap-server
%doc initscript/README.stap-server
%doc README README.unprivileged AUTHORS NEWS COPYING


%files devel -f systemtap.lang
%{_bindir}/stap
%{_bindir}/stap-prep
%{_bindir}/stap-report
%dir %{_datadir}/systemtap
%{_datadir}/systemtap/runtime
%{_datadir}/systemtap/tapset
%{_mandir}/man1/stap.1*
%{_mandir}/man7/error*
%{_mandir}/man7/stappaths.7*
%{_mandir}/man7/warning*
%doc README README.unprivileged AUTHORS NEWS COPYING
%if %{with_bundled_elfutils}
%dir %{_libdir}/systemtap
%{_libdir}/systemtap/lib*.so*
%endif
%if %{with_emacsvim}
%{_emacs_sitelispdir}/*.el*
%{_emacs_sitestartdir}/systemtap-init.el
%{_datadir}/vim/vimfiles/*/*.vim
%endif


%files runtime -f systemtap.lang
%defattr(-,root,root)
%attr(4110,root,stapusr) %{_bindir}/staprun
%{_bindir}/stapsh
%{_bindir}/stap-merge
%{_bindir}/stap-report
%if %{with_dyninst}
%{_bindir}/stapdyn
%endif
%dir %{_libexecdir}/systemtap
%{_libexecdir}/systemtap/stapio
%{_libexecdir}/systemtap/stap-authorize-cert
%if %{with_crash}
%dir %{_libdir}/systemtap
%{_libdir}/systemtap/staplog.so*
%endif
%{_mandir}/man7/error*
%{_mandir}/man7/stappaths.7*
%{_mandir}/man7/warning*
%{_mandir}/man8/staprun.8*
%doc README README.security AUTHORS NEWS COPYING


%files client -f systemtap.lang
%defattr(-,root,root)
%doc README README.unprivileged AUTHORS NEWS COPYING examples
%if %{with_docs}
%doc docs.installed/*.pdf
%doc docs.installed/tapsets/*.html
%if %{with_publican}
%doc docs.installed/SystemTap_Beginners_Guide
%endif
%endif
%{_bindir}/stap
%{_bindir}/stap-prep
%{_bindir}/stap-report
%{_mandir}/man1/stap.1*
%{_mandir}/man1/stap-merge.1*
%{_mandir}/man3/*
%{_mandir}/man7/error*
%{_mandir}/man7/stappaths.7*
%{_mandir}/man7/warning*
%dir %{_datadir}/systemtap
%{_datadir}/systemtap/tapset



%files initscript
%defattr(-,root,root)
%{_sysconfdir}/rc.d/init.d/systemtap
%dir %{_sysconfdir}/systemtap
%dir %{_sysconfdir}/systemtap/conf.d
%dir %{_sysconfdir}/systemtap/script.d
%config(noreplace) %{_sysconfdir}/systemtap/config
%dir %{_localstatedir}/cache/systemtap
%ghost %{_localstatedir}/run/systemtap
%doc initscript/README.systemtap


%files sdt-devel -f systemtap.lang
%defattr(-,root,root)
%{_bindir}/dtrace
%{_includedir}/sys/sdt.h
%{_includedir}/sys/sdt-config.h
%{_mandir}/man1/dtrace.1*
%doc README AUTHORS NEWS COPYING


%files testsuite
%defattr(-,root,root)
%dir %{_datadir}/systemtap
%{_datadir}/systemtap/testsuite


# ------------------------------------------------------------------------

%changelog
* Wed Feb 13 2013 Serguei Makarov <smakarov@redhat.com> - 2.1-1
- Upstream release.

* Tue Oct 09 2012 Josh Stone <jistone@redhat.com> - 2.0-1
- Upstream release.

* Fri Jul 13 2012 Peter Robinson <pbrobinson@fedoraproject.org>
- Fix ifarch statement
- use file based requires for glibc-devel on x86_64 so that we work in koji

* Sun Jun 17 2012 Frank Ch. Eigler <fche@redhat.com> - 1.8-1
- Upstream release.

* Wed Feb 01 2012 Frank Ch. Eigler <fche@redhat.com> - 1.7-1
- Upstream release.

* Fri Jan 13 2012 David Smith <dsmith@redhat.com> - 1.6-2
- Fixed /bin/mktemp require.

* Mon Jul 25 2011 Stan Cox <scox@redhat.com> - 1.6-1
- Upstream release.

* Tue May 23 2011 Stan Cox <scox@redhat.com> - 1.5-1
- Upstream release.

* Mon Jan 17 2011 Frank Ch. Eigler <fche@redhat.com> - 1.4-1
- Upstream release.

* Wed Jul 21 2010 Josh Stone <jistone@redhat.com> - 1.3-1
- Upstream release.

* Mon Mar 22 2010 Frank Ch. Eigler <fche@redhat.com> - 1.2-1
- Upstream release.

* Mon Dec 21 2009 David Smith <dsmith@redhat.com> - 1.1-1
- Upstream release.

* Tue Sep 22 2009 Josh Stone <jistone@redhat.com> - 1.0-1
- Upstream release.

* Tue Aug  4 2009 Josh Stone <jistone@redhat.com> - 0.9.9-1
- Upstream release.

* Thu Jun 11 2009 Josh Stone <jistone@redhat.com> - 0.9.8-1
- Upstream release.

* Thu Apr 23 2009 Josh Stone <jistone@redhat.com> - 0.9.7-1
- Upstream release.

* Fri Mar 27 2009 Josh Stone <jistone@redhat.com> - 0.9.5-1
- Upstream release.

* Wed Mar 18 2009 Will Cohen <wcohen@redhat.com> - 0.9-2
- Add location of man pages.

* Tue Feb 17 2009 Frank Ch. Eigler <fche@redhat.com> - 0.9-1
- Upstream release.

* Thu Nov 13 2008 Frank Ch. Eigler <fche@redhat.com> - 0.8-1
- Upstream release.

* Tue Jul 15 2008 Frank Ch. Eigler <fche@redhat.com> - 0.7-1
- Upstream release.

* Fri Feb  1 2008 Frank Ch. Eigler <fche@redhat.com> - 0.6.1-3
- Add zlib-devel to buildreq; missing from crash-devel
- Process testsuite .stp files for #!stap->#!/usr/bin/stap

* Fri Jan 18 2008 Frank Ch. Eigler <fche@redhat.com> - 0.6.1-1
- Add crash-devel buildreq to build staplog.so crash(8) module.
- Many robustness & functionality improvements:

* Wed Dec  5 2007 Will Cohen <wcohen@redhat.com> - 0.6-2
- Correct Source to point to location contain code.

* Thu Aug  9 2007 David Smith <dsmith@redhat.com> - 0.6-1
- Bumped version, added libcap-devel BuildRequires.

* Wed Jul 11 2007 Will Cohen <wcohen@redhat.com> - 0.5.14-2
- Fix Requires and BuildRequires for sqlite.

* Tue Jul  2 2007 Frank Ch. Eigler <fche@redhat.com> - 0.5.14-1
- Many robustness improvements: 1117, 1134, 1305, 1307, 1570, 1806,
  2033, 2116, 2224, 2339, 2341, 2406, 2426, 2438, 2583, 3037,
  3261, 3282, 3331, 3428 3519, 3545, 3625, 3648, 3880, 3888, 3911,
  3952, 3965, 4066, 4071, 4075, 4078, 4081, 4096, 4119, 4122, 4127,
  4146, 4171, 4179, 4183, 4221, 4224, 4254, 4281, 4319, 4323, 4326,
  4329, 4332, 4337, 4415, 4432, 4444, 4445, 4458, 4467, 4470, 4471,
  4518, 4567, 4570, 4579, 4589, 4609, 4664

* Mon Mar 26 2007 Frank Ch. Eigler <fche@redhat.com> - 0.5.13-1
- An emergency / preliminary refresh, mainly for compatibility
  with 2.6.21-pre kernels.

* Mon Jan  1 2007 Frank Ch. Eigler <fche@redhat.com> - 0.5.12-1
- Many changes, see NEWS file.

* Tue Sep 26 2006 David Smith <dsmith@redhat.com> - 0.5.10-1
- Added 'systemtap-runtime' subpackage.

* Wed Jul 19 2006 Roland McGrath <roland@redhat.com> - 0.5.9-1
- PRs 2669, 2913

* Fri Jun 16 2006 Roland McGrath <roland@redhat.com> - 0.5.8-1
- PRs 2627, 2520, 2228, 2645

* Fri May  5 2006 Frank Ch. Eigler <fche@redhat.com> - 0.5.7-1
- PRs 2511 2453 2307 1813 1944 2497 2538 2476 2568 1341 2058 2220 2437
  1326 2014 2599 2427 2438 2465 1930 2149 2610 2293 2634 2506 2433

* Tue Apr  4 2006 Roland McGrath <roland@redhat.com> - 0.5.5-1
- Many changes, affected PRs include: 2068, 2293, 1989, 2334,
  1304, 2390, 2425, 953.

* Wed Feb  1 2006 Frank Ch. Eigler <fche@redhat.com> - 0.5.4-1
- PRs 1916, 2205, 2142, 2060, 1379

* Mon Jan 16 2006 Roland McGrath <roland@redhat.com> - 0.5.3-1
- Many changes, affected PRs include: 2056, 1144, 1379, 2057,
  2060, 1972, 2140, 2148

* Mon Dec 19 2005 Roland McGrath <roland@redhat.com> - 0.5.2-1
- Fixed build with gcc 4.1, various tapset changes.

* Wed Dec  7 2005 Roland McGrath <roland@redhat.com> - 0.5.1-1
- elfutils update, build changes

* Fri Dec 02 2005  Frank Ch. Eigler  <fche@redhat.com> - 0.5-1
- Many fixes and improvements: 1425, 1536, 1505, 1380, 1329, 1828, 1271,
  1339, 1340, 1345, 1837, 1917, 1903, 1336, 1868, 1594, 1564, 1276, 1295

* Mon Oct 31 2005 Roland McGrath <roland@redhat.com> - 0.4.2-1
- Many fixes and improvements: PRs 1344, 1260, 1330, 1295, 1311, 1368,
  1182, 1131, 1332, 1366, 1456, 1271, 1338, 1482, 1477, 1194.

* Wed Sep 14 2005 Roland McGrath <roland@redhat.com> - 0.4.1-1
- Many fixes and improvements since 0.2.2; relevant PRs include:
  1122, 1134, 1155, 1172, 1174, 1175, 1180, 1186, 1187, 1191, 1193, 1195,
  1197, 1205, 1206, 1209, 1213, 1244, 1257, 1258, 1260, 1265, 1268, 1270,
  1289, 1292, 1306, 1335, 1257

* Wed Sep  7 2005 Frank Ch. Eigler <fche@redhat.com>
- Bump version.

* Wed Aug 16 2005 Frank Ch. Eigler <fche@redhat.com>
- Bump version.

* Wed Aug  3 2005 Martin Hunt <hunt@redhat.com> - 0.2.2-1
- Add directory /var/cache/systemtap
- Add stp_check to /usr/libexec/systemtap

* Wed Aug  3 2005 Roland McGrath <roland@redhat.com> - 0.2.1-1
- New version 0.2.1, various fixes.

* Fri Jul 29 2005 Roland McGrath <roland@redhat.com> - 0.2-1
- New version 0.2, requires elfutils 0.111

* Mon Jul 25 2005 Roland McGrath <roland@redhat.com>
- Clean up spec file, build bundled elfutils.

* Thu Jul 21 2005 Martin Hunt <hunt@redhat.com>
- Set Version to use version from autoconf.
- Fix up some of the path names.
- Add Requires and BuildRequires.

* Wed Jul 19 2005 Will Cohen <wcohen@redhat.com>
- Initial creation of RPM.
