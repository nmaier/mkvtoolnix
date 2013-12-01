# SPEC file for CentOS 6

# Note that CentOS does not include a Boost version new enough to
# compile MKVToolNix with. Therefore this spec relies on you having
# compiled and installed an appropriate version of it yourself. You
# should make use of the EXTRA_CONFIGURE_ARGS environment variable.

Summary: mkvtoolnix
Name: mkvtoolnix
Version: 6.6.0
Release: 1
License: GPL
Group: Multimedia
Source: %{name}-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{version}-tmproot
BuildRequires: gcc gcc-c++ make flac-devel libogg-devel libvorbis-devel wxGTK-devel file-devel zlib-devel gettext-devel ruby libcurl-devel

%description
Matroska video utilities.

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README ChangeLog NEWS TODO
%{_bindir}/*
%{_datadir}/man/man1/*
%{_datadir}/man/de/man1/*
%{_datadir}/man/ja/man1/*
%{_datadir}/man/nl/man1/*
%{_datadir}/man/uk/man1/*
%{_datadir}/man/zh_CN/man1/*
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/16x16/apps/*.png
%{_datadir}/icons/hicolor/24x24/apps/*.png
%{_datadir}/icons/hicolor/32x32/apps/*.png
%{_datadir}/icons/hicolor/48x48/apps/*.png
%{_datadir}/icons/hicolor/64x64/apps/*.png
%{_datadir}/icons/hicolor/96x96/apps/*.png
%{_datadir}/icons/hicolor/128x128/apps/*.png
%{_datadir}/icons/hicolor/256x256/apps/*.png
%{_datadir}/mime/packages/*.xml
%{_datadir}/locale/cs/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/de/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/es/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/eu/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/fr/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/it/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/ja/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/lt/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/nl/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/pl/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/pt/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/ru/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/tr/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/uk/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_CN/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_TW/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/doc/mkvtoolnix/guide

%changelog
* Sat Jan 2 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
- set this thing up

%prep
%setup
%configure --prefix=%{_prefix} $EXTRA_CONFIGURE_ARGS

%build
./drake

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
./drake DESTDIR=%{?buildroot} install

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
