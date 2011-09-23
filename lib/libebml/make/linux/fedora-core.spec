# SPEC file for libebml on (at least) Fedora Core 1, 2, 3

Name: 		libebml
Version: 1.2.2
Release: 1
License: 	LGPL
Summary:	Extensible Binary Meta Language
Group:		System Environment/Libraries
URL: 		http://ebml.sourceforge.net/
Vendor:         Moritz Bunkus <moritz@bunkus.org>
Source:		http://dl.matroska.org/downloads/%{name}/%{name}-%{version}.tar.bz2
BuildRoot: 	%{_tmppath}/%{name}-root

%description
EBML was designed to be a simplified binary extension of XML for
the purpose of storing and manipulating data in a hierarchical
form with variable field lengths.
It uses the same paradigms as XML files, ie syntax and semantic
are separated. So a generic EBML library could read any format
based on it. The interpretation of data is up to a specific
application that knows how each elements (equivalent of XML tag)
has to be handled.

%package devel
Summary:	Extensible Binary Meta Language headers/development files
Group:		Development/Libraries

%description devel
EBML was designed to be a simplified binary extension of XML for
the purpose of storing and manipulating data in a hierarchical
form with variable field lengths.
It uses the same paradigms as XML files, ie syntax and semantic
are separated. So a generic EBML library could read any format
based on it. The interpretation of data is up to a specific
application that knows how each elements (equivalent of XML tag)
has to be handled.

%prep
%setup -q

%build
cd make/linux
CFLAGS="$RPM_OPT_FLAGS" \
make \
prefix="%{_prefix}" staticlib
cd ../..

%install
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT
cd make/linux
make prefix=$RPM_BUILD_ROOT/%{_prefix} libdir=$RPM_BUILD_ROOT/%{_libdir} install_staticlib install_headers
cd ../..

%clean
[ "$RPM_BUILD_ROOT" != "/" ] && rm -rf $RPM_BUILD_ROOT

%files devel
%defattr(-, root, root)
%{_includedir}/ebml/*.h
%{_includedir}/ebml/c/*.h
%{_libdir}/libebml.a

%changelog
* Sat Apr 16 2005 Moritz Bunkus <moritz@bunkus.org>
- updated for the new libebml build targets
* Fri May 15 2003 Ronald Bultje <rbultje@ronald.bitfreak.net>
- create spec file
