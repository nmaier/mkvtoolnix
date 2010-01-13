# SPEC file for (at least) Fedora Core 5 and 6

Summary: mkvtoolnix
Name: mkvtoolnix
Version: 3.0.0
Release: 1
License: GPL
Group: Multimedia
Source: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-tmproot
BuildRequires: libebml-devel >= 0.7.7
BuildRequires: libmatroska-devel >= 0.8.1
BuildRequires: gcc-c++ gcc gcc-c++ make flac-devel libogg-devel libvorbis-devel wxGTK-devel >= 2.6 boost-devel file lzo-devel bzip2-devel zlib-devel

%description
Matroska video utilities.

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README ChangeLog NEWS TODO
%{_bindir}/*
%{_datadir}/man/man1/*
%{_datadir}/man/ja/man1/*
%{_datadir}/mkvtoolnix
%{_datadir}/locale/de/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/ja/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_CN/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_TW/LC_MESSAGES/mkvtoolnix.mo

%changelog
* Sat Jan 2 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
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
