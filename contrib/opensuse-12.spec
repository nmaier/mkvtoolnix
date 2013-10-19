#
# spec file for package mkvtoolnix
# works with openSUSE 11.4 and newer
#

BuildRequires: flac flac-devel gcc-c++ gtk2 gtk2-devel libogg0 libogg-devel libstdc++-devel libvorbis libvorbis-devel pkgconfig boost-devel file-devel ruby libcurl-devel wxWidgets-devel make gettext-tools

Name:         mkvtoolnix
URL:          http://www.bunkus.org/videotools/mkvtoolnix/
Version: 6.5.0
Release: 1
Summary:      tools to create, alter and inspect Matroska files
License:      GPL
Group:        Productivity/Multimedia/Other
Source:       %{name}-%{version}.tar.xz
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

%build
export CFLAGS="$RPM_OPT_FLAGS"
./configure --prefix=/usr --mandir=/usr/share/man $EXTRA_CONFIGURE_ARGS
./drake

%install
sed -i -e 's/^Exec=mmg/Exec=mkvmerge-gui/' share/desktop/mkvmergeGUI.desktop
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

%changelog -n mkvtoolnix
* Mon Sep 13 2004 - seife@suse.de
- renamed mmg to mkvmerge-gui (conflict with mupad)
* Wed Sep 01 2004 - seife@suse.de
- initial submission
