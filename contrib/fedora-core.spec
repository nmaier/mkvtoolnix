# SPEC file for (at least) Fedora Core 1, 2 and 3

Summary: mkvtoolnix
Name: mkvtoolnix
Version: 1.5.5
Release: 1
Copyright: GPL
Group: Multimedia
Source: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-tmproot
BuildRequires: libebml-devel >= 0.7.2
BuildRequires: libmatroska-devel >= 0.7.4
BuildRequires: gcc-c++

%description
Matroska video utilities.

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README ChangeLog NEWS TODO
%doc doc/mkvmerge-gui.html
%{_bindir}/*
%{_datadir}/man/man1/*

%changelog
* Sat Jan 2 2004 Ronald Bultje <rbultje@ronald.bitfreak.net
- set this thing up

%prep
%setup
%configure --prefix=%{_prefix}

%build
make

%install  
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
%makeinstall

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
