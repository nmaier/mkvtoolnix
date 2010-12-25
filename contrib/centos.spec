# SPEC file for (at least) Fedora Core 5 and 6

Summary: mkvtoolnix
Name: mkvtoolnix
Version: 4.4.0
Release: 1
License: GPL
Group: Multimedia
Source: %{name}-%{version}.tar.bz2
BuildRoot: %{_tmppath}/%{name}-%{version}-tmproot
BuildRequires: libebml-devel >= 1.0.0
BuildRequires: libmatroska-devel >= 1.0.0
BuildRequires: gcc-c++ gcc gcc-c++ make libogg-devel libvorbis-devel wxGTK-devel >= 2.8 boost-devel file lzo-devel bzip2-devel zlib-devel expat-devel gettext-devel ruby curl-devel

%description
Matroska video utilities.

%files
%defattr(-, root, root)
%doc AUTHORS COPYING README ChangeLog NEWS TODO
%{_bindir}/*
%{_datadir}/man/man1/*
%{_datadir}/man/ja/man1/*
%{_datadir}/man/nl/man1/*
%{_datadir}/man/zh_CN/man1/*
%{_datadir}/mkvtoolnix
%{_datadir}/applications/*.desktop
%{_datadir}/icons/hicolor/32x32/apps/*.png
%{_datadir}/icons/hicolor/64x64/apps/*.png
%{_datadir}/mime/packages/*.xml
%{_datadir}/locale/de/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/es/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/fr/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/ja/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/nl/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/ru/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/uk/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_CN/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_TW/LC_MESSAGES/mkvtoolnix.mo

%changelog
* Sat Jan 2 2004 Ronald Bultje <rbultje@ronald.bitfreak.net>
- set this thing up

%prep
%setup
export CFLAGS="`echo $CFLAGS | sed -e 's/-O2//g'` -O1"
export CXXFLAGS="`echo $CXXFLAGS | sed -e 's/-O2//g'` -O1"
%configure --prefix=%{_prefix} --without-flac --disable-optimization $EXTRA_CONFIGURE_ARGS

%build
./drake

%install
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
./drake DESTDIR=%{?buildroot} install

%clean
[ -n "$RPM_BUILD_ROOT" -a "$RPM_BUILD_ROOT" != / ] && rm -rf $RPM_BUILD_ROOT
