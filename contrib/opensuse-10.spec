#
# spec file for package mkvtoolnix
#

BuildRequires: libebml >= 0.7.7
BuildRequires: libmatroska >= 0.8.1
BuildRequires: expat flac flac-devel gcc-c++ gtk2 gtk2-devel libogg libogg-devel libstdc++-devel libvorbis libvorbis-devel lzo lzo-devel pkgconfig wxGTK >= 2.6 wxGTK-devel >= 2.6 wxGTK-gl boost-devel file-devel

Name:         mkvtoolnix
URL:          http://www.bunkus.org/videotools/mkvtoolnix/
Version: 2.8.0
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
command line tools mkvextract, mkvinfo, mkvmerge and a graphical
frontend for them, mkvmerge-gui.



Authors:
--------
    Moritz Bunkus <moritz@bunkus.org>

%prep
rm -rf $RPM_BUILD_ROOT
%setup
%patch -p1

%build
export CFLAGS="$RPM_OPT_FLAGS"
./configure --prefix=/usr --mandir=/usr/share/man
make

%install
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%post
%run_ldconfig

%postun
%run_ldconfig

%files
%defattr (-,root,root)
/usr/bin/mkvextract
/usr/bin/mkvinfo
/usr/bin/mkvmerge
/usr/bin/mkvmerge-gui
/usr/share/man/man1/mkvextract.1.gz
/usr/share/man/man1/mkvinfo.1.gz
/usr/share/man/man1/mkvmerge.1.gz
/usr/share/man/man1/mkvmerge-gui.1.gz
/usr/share/mkvtoolnix/*
/usr/share/locale/de/LC_MESSAGES/mkvtoolnix.mo
/usr/share/locale/ja/LC_MESSAGES/mkvtoolnix.mo
/usr/share/locale/zh/LC_MESSAGES/mkvtoolnix.mo
%doc AUTHORS ChangeLog COPYING README

%changelog -n mkvtoolnix
* Mon Sep 13 2004 - seife@suse.de
- renamed mmg to mkvmerge-gui (conflict with mupad)
* Wed Sep 01 2004 - seife@suse.de
- initial submission
