#
# spec file for package mkvtoolnix
#
# Copyright (c) 2004 SUSE LINUX AG, Nuernberg, Germany.
# This file and all modifications and additions to the pristine
# package are under the same license as the package itself.
#
# Please submit bugfixes or comments via http://www.suse.de/feedback/
#

# neededforbuild  expat flac flac-devel gcc-c++ gtk2 gtk2-devel libebml libmatroska libmspack libogg libogg-devel libpng libpng-devel libstdc++-devel libtiff libtiff-devel libvorbis libvorbis-devel lzo lzo-devel pkgconfig unixODBC wxGTK wxGTK-compat wxGTK-devel wxGTK-gl

BuildRequires: aaa_base attr bash bind-utils bison bzip2 coreutils cpio cpp cracklib db devs diffutils file filesystem fillup findutils flex gawk glibc glibc-devel glibc-locale gpm grep groff gzip info insserv less libacl libattr libgcc libstdc++ libxcrypt m4 make man mktemp ncurses net-tools netcfg openldap2-client openssl pam pam-modules patch permissions popt readline sed syslogd sysvinit tar timezone util-linux vim zlib zlib-devel autoconf automake binutils expat flac flac-devel fontconfig freetype2 gcc gcc-c++ gdbm gettext libebml libjpeg libjpeg-devel libmatroska libogg libogg-devel libpng libstdc++-devel libtiff libtiff-devel libtool libvorbis libvorbis-devel libxml2 lzo lzo-devel perl rpm wxGTK wxGTK-devel

Name:         mkvtoolnix
URL:          http://www.bunkus.org/videotools/mkvtoolnix/
Version: 1.5.6
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
/usr/bin/base64tool
/usr/bin/mkvextract
/usr/bin/mkvinfo
/usr/bin/mkvmerge
/usr/bin/mkvmerge-gui
/usr/share/man/man1/base64tool.1.gz
/usr/share/man/man1/mkvextract.1.gz
/usr/share/man/man1/mkvinfo.1.gz
/usr/share/man/man1/mkvmerge.1.gz
/usr/share/man/man1/mkvmerge-gui.1.gz
%doc AUTHORS ChangeLog COPYING README

%changelog -n mkvtoolnix
* Mon Sep 13 2004 - seife@suse.de
- renamed mmg to mkvmerge-gui (conflict with mupad)
* Wed Sep 01 2004 - seife@suse.de
- initial submission
