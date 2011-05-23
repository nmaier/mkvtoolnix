#
# spec file for package mkvtoolnix
# works with openSUSE 11.4 and newer
#

BuildRequires: expat libexpat-devel flac flac-devel gcc-c++ gtk2 gtk2-devel libogg libogg-devel libstdc++-devel libvorbis libvorbis-devel lzo lzo-devel pkgconfig boost-devel file-devel ruby libcurl-devel
BuildRequires: wxWidgets-devel patch make gettext-tools

Name:         mkvtoolnix
URL:          http://www.bunkus.org/videotools/mkvtoolnix/
Version: 4.8.0
Release: 1
Summary:      tools to create, alter and inspect Matroska files
License:      GPL
Group:        Productivity/Multimedia/Other
Source:       %{name}-%{version}.tar.bz2
Patch:        suse-mmg-rename.diff
Summary:      tools to create, alter and inspect Matroska files
BuildRoot:    %{_tmppath}/%{name}-%{version}-build
Prefix:       /usr

%description
Tools to create and manipulate Matroska files (extensions .mkv and
.mka), a new container format for audio and video files. Includes
command line tools mkvextract, mkvinfo, mkvmerge, mkvpropedit and a
graphical frontend for them, mkvmerge-gui.



Authors:
--------
    Moritz Bunkus <moritz@bunkus.org>

%prep
rm -rf $RPM_BUILD_ROOT
%setup
%patch -p1

%build
export CFLAGS="$RPM_OPT_FLAGS"
./configure --prefix=/usr --mandir=/usr/share/man $EXTRA_CONFIGURE_ARGS
./drake

%install
./drake DESTDIR=$RPM_BUILD_ROOT MMG_BIN=mkvmerge-gui install

%clean
rm -rf $RPM_BUILD_ROOT

%post
%run_ldconfig

%postun
%run_ldconfig

%files
%defattr (-,root,root)
%doc AUTHORS COPYING README ChangeLog NEWS TODO
%{_bindir}/*
%{_datadir}/man/man1/*
%{_datadir}/man/ja/man1/*
%{_datadir}/man/nl/man1/*
%{_datadir}/man/zh_CN/man1/*
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
%{_datadir}/locale/tr/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/uk/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_CN/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/locale/zh_TW/LC_MESSAGES/mkvtoolnix.mo
%{_datadir}/doc/mkvtoolnix/guide

%changelog -n mkvtoolnix
* Mon Sep 13 2004 - seife@suse.de
- renamed mmg to mkvmerge-gui (conflict with mupad)
* Wed Sep 01 2004 - seife@suse.de
- initial submission
