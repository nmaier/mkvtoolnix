dnl PATH_AVILIB([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for avilib, and define AVILIB_CFLAGS and AVILIB_LIBS
dnl
AC_DEFUN(PATH_AVILIB,
[dnl 
dnl Get the cflags and libraries
dnl

AVILIB_CFLAGS="-Iavilib -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
AVILIB_CXXFLAGS="-Iavilib -D_LARGEFILE64_SOURCE -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64"
AVILIB_LIBS="-Lavilib -lavi"

  AC_SUBST(AVILIB_CFLAGS)
  AC_SUBST(AVILIB_CXXFLAGS)
  AC_SUBST(AVILIB_LIBS)
])


# Configure paths for libogg
# Jack Moffitt <jack@icecast.org> 10-21-2000
# Shamelessly stolen from Owen Taylor and Manish Singh

dnl XIPH_PATH_OGG([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libogg, and define OGG_CFLAGS and OGG_LIBS
dnl
AC_DEFUN(XIPH_PATH_OGG,
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(ogg-prefix,[  --with-ogg-prefix=PFX         Prefix where libogg is installed (optional)], ogg_prefix="$withval", ogg_prefix="")
AC_ARG_ENABLE(oggtest, [  --disable-oggtest             Do not try to compile and run a test Ogg program],, enable_oggtest=yes)

  if test "x$ogg_prefix" != "x"; then
    ogg_args="$ogg_args --prefix=$ogg_prefix"
    OGG_CFLAGS="-I$ogg_prefix/include"
    OGG_LIBS="-L$ogg_prefix/lib"
  elif test "x$prefix" != "xNONE"; then
    ogg_args="$ogg_args --prefix=$prefix"
    OGG_CFLAGS="-I$prefix/include"
    OGG_LIBS="-L$prefix/lib"
  fi

  OGG_LIBS="$OGG_LIBS -logg"

  AC_MSG_CHECKING(for Ogg)
  no_ogg=""


  if test "x$enable_oggtest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $OGG_CFLAGS"
    LIBS="$LIBS $OGG_LIBS"
dnl
dnl Now check if the installed Ogg is sufficiently new.
dnl
      rm -f conf.oggtest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ogg/ogg.h>

int main ()
{
  system("touch conf.oggtest");
  return 0;
}

],, no_ogg=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_ogg" = "x" ; then
     AC_MSG_RESULT(yes)
     echo '#define HAVE_OGG 1' > config.h
     ifelse([$1], , :, [$1])     
  else
     AC_MSG_RESULT(no)
     echo '/*#define HAVE_OGG 1*/' > config.h
     if test -f conf.oggtest ; then
       :
     else
       echo "*** Could not run Ogg test program, checking why..."
       CFLAGS="$CFLAGS $OGG_CFLAGS"
       LIBS="$LIBS $OGG_LIBS"
       AC_TRY_LINK([
#include <stdio.h>
#include <ogg/ogg.h>
],     [ return 0; ],
       [ echo "*** The test program compiled, but did not run. This usually means"
       echo "*** that the run-time linker is not finding Ogg or finding the wrong"
       echo "*** version of Ogg. If it is not finding Ogg, you'll need to set your"
       echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
       echo "*** to the installed location  Also, make sure you have run ldconfig if that"
       echo "*** is required on your system"
       echo "***"
       echo "*** If you have an old version installed, it is best to remove it, although"
       echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
       [ echo "*** The test program failed to compile or link. See the file config.log for the"
       echo "*** exact error that occured. This usually means Ogg was incorrectly installed"
       echo "*** or that you have moved Ogg since it was installed. In the latter case, you"
       echo "*** may want to edit the ogg-config script: $OGG_CONFIG" ])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
     OGG_CFLAGS=""
     OGG_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(OGG_CFLAGS)
  AC_SUBST(OGG_LIBS)
  rm -f conf.oggtest
])
# Configure paths for libvorbis
# Jack Moffitt <jack@icecast.org> 10-21-2000
# Shamelessly stolen from Owen Taylor and Manish Singh

dnl XIPH_PATH_VORBIS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libvorbis, and define VORBIS_CFLAGS and VORBIS_LIBS
dnl
AC_DEFUN(XIPH_PATH_VORBIS,
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(vorbis-prefix,[  --with-vorbis-prefix=PFX      Prefix where libvorbis is installed (optional)], vorbis_prefix="$withval", vorbis_prefix="")
AC_ARG_ENABLE(vorbistest, [  --disable-vorbistest          Do not try to compile and run a test Vorbis program],, enable_vorbistest=yes)

  if test "x$vorbis_prefix" != "x" ; then
    vorbis_args="$vorbis_args --prefix=$vorbis_prefix"
    VORBIS_CFLAGS="-I$vorbis_prefix/include"
    VORBIS_LIBDIR="-L$vorbis_prefix/lib"
  elif test "x$prefix" != "xNONE"; then
    vorbis_args="$vorbis_args --prefix=$prefix"
    VORBIS_CFLAGS="-I$prefix/include"
    VORBIS_LIBDIR="-L$prefix/lib"
  fi

  VORBIS_LIBS="$VORBIS_LIBDIR -lvorbis -lm"
  VORBISFILE_LIBS="-lvorbisfile"

  AC_MSG_CHECKING(for Vorbis)
  no_vorbis=""


  if test "x$enable_vorbistest" = "xyes" ; then
    ac_save_CFLAGS="$CFLAGS"
    ac_save_LIBS="$LIBS"
    CFLAGS="$CFLAGS $VORBIS_CFLAGS"
    LIBS="$LIBS $VORBIS_LIBS $OGG_LIBS"
dnl
dnl Now check if the installed Vorbis is sufficiently new.
dnl
      rm -f conf.vorbistest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vorbis/codec.h>

int main ()
{
  system("touch conf.vorbistest");
  return 0;
}

],, no_vorbis=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_vorbis" = "x" ; then
     AC_MSG_RESULT(yes)
     echo '#define HAVE_VORBIS 1' >> config.h
     if test "x$no_vorbis" = "x" ; then
        echo '#define HAVE_OGGVORBIS 1' >> config.h
     else
        echo '/*#define HAVE_OGGVORBIS 1*/' >> config.h
     fi
     ifelse([$1], , :, [$1])     
  else
     AC_MSG_RESULT(no)
     echo '/*#define HAVE_VORBIS 1*/' >> config.h
     echo '/*#define HAVE_OGGVORBIS 1*/' >> config.h
     if test -f conf.vorbistest ; then
       :
     else
       echo "*** Could not run Vorbis test program, checking why..."
       CFLAGS="$CFLAGS $VORBIS_CFLAGS"
       LIBS="$LIBS $VORBIS_LIBS $OGG_LIBS"
       AC_TRY_LINK([
#include <stdio.h>
#include <vorbis/codec.h>
],     [ return 0; ],
       [ echo "*** The test program compiled, but did not run. This usually means"
       echo "*** that the run-time linker is not finding Vorbis or finding the wrong"
       echo "*** version of Vorbis. If it is not finding Vorbis, you'll need to set your"
       echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
       echo "*** to the installed location  Also, make sure you have run ldconfig if that"
       echo "*** is required on your system"
       echo "***"
       echo "*** If you have an old version installed, it is best to remove it, although"
       echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
       [ echo "*** The test program failed to compile or link. See the file config.log for the"
       echo "*** exact error that occured. This usually means Vorbis was incorrectly installed"
       echo "*** or that you have moved Vorbis since it was installed." ])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
     VORBIS_CFLAGS=""
     VORBIS_LIBS=""
     VORBISFILE_LIBS=""
     ifelse([$2], , :, [$2])
  fi
  AC_SUBST(VORBIS_CFLAGS)
  AC_SUBST(VORBIS_LIBS)
  AC_SUBST(VORBISFILE_LIBS)
  rm -f conf.vorbistest
])

dnl AC_TRY_CFLAGS (CFLAGS, [ACTION-IF-WORKS], [ACTION-IF-FAILS])
dnl check if $CC supports a given set of cflags
AC_DEFUN([AC_TRY_CFLAGS],
    [AC_MSG_CHECKING([if $CC supports $1 flags])
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$1"
    AC_TRY_COMPILE([],[],[ac_cv_try_cflags_ok=yes],[ac_cv_try_cflags_ok=no])
    CFLAGS="$SAVE_CFLAGS"
    AC_MSG_RESULT([$ac_cv_try_cflags_ok])
    if test x"$ac_cv_try_cflags_ok" = x"yes"; then
        ifelse([$2],[],[:],[$2])
    else
        ifelse([$3],[],[:],[$3])
    fi])

AC_DEFUN(PATH_DEBUG,[
AC_ARG_ENABLE([debug],
    [  --enable-debug                compile with debug information])
if test x"$enable_debug" = x"yes"; then
    dnl debug information
    DEBUG_CFLAGS="-g"
    echo '#define DEBUG' >> config.h
else
    DEBUG_CFLAGS=""
    echo '/*#define DEBUG*/' >> config.h
fi
  AC_SUBST(DEBUG_CFLAGS)
])

AC_DEFUN(PATH_PROFILING,[
AC_ARG_ENABLE([profiling],
    [  --enable-profiling            compile with profiling information])
if test x"$enable_profiling" = x"yes"; then
    dnl profiling information
    PROFILING_CFLAGS="-pg"
    PROFILING_LIBS=""
else
    PROFILING_CFLAGS=""
    PROFILING_LIBS=""
fi
  AC_SUBST(PROFILING_CFLAGS)
  AC_SUBST(PROFILING_LIBS)
])

# Configure paths for libebml

dnl PATH_EBML([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libebml, and define EBML_CFLAGS and EBML_LIBS
dnl
AC_DEFUN(PATH_EBML,
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(ebml-prefix,[  --with-ebml-prefix=PFX        Prefix where libebml is installed (optional)], ebml_prefix="$withval", ebml_prefix="")
AC_ARG_WITH(ebml-include,[  --with-ebml-include=DIR       Path to where the libebml include files installed (optional)], ebml_include="$withval", ebml_include="")
AC_ARG_WITH(ebml-lib,[  --with-ebml-lib=DIR           Path to where the libebml library installed (optional)], ebml_lib="$withval", ebml_lib="")
AC_ARG_ENABLE(ebmltest, [  --disable-ebmltest            Do not try to compile and run a test EBML program],, enable_ebmltest=yes)

  if test "x$ebml_prefix" != "x"; then
    ebml_args="$ebml_args --prefix=$ebml_prefix"
    if test "x$ebml_include" != "x"; then
      EBML_CFLAGS="-I$ebml_include"
    else
      EBML_CFLAGS="-I$ebml_prefix/include"
    fi
    if test "x$ebml_lib" != "x"; then
      EBML_LIBS="-L$ebml_lib"
    else
      EBML_LIBS="-L$ebml_prefix/lib"
    fi
  elif test "x$prefix" != "xNONE"; then
    ebml_args="$ebml_args --prefix=$prefix"
    if test "x$ebml_include" != "x"; then
      EBML_CFLAGS="-I$ebml_include"
    else
      EBML_CFLAGS="-I$prefix/include"
    fi
    if test "x$ebml_lib" != "x"; then
      EBML_LIBS="-L$ebml_lib"
    else
      EBML_LIBS="-L$prefix/lib"
    fi
  else
    if test "x$ebml_include" != "x"; then
      EBML_CFLAGS="-I$ebml_include"
    fi
    if test "x$ebml_lib" != "x"; then
      EBML_LIBS="-L$ebml_lib"
    else
      EBML_LIBS="-L/usr/local/lib"
    fi
  fi

  EBML_LIBS="$EBML_LIBS -lebml"

  AC_MSG_CHECKING(for EBML)
  no_ebml=""


  if test "x$enable_ebmltest" = "xyes" ; then
    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LIBS="$LIBS"
    CXXFLAGS="$CXXFLAGS $EBML_CFLAGS"
    LIBS="$LIBS $EBML_LIBS"
dnl
dnl Now check if the installed EBML is sufficiently new.
dnl
      rm -f conf.ebmltest
      AC_LANG_CPLUSPLUS
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ebml/EbmlVersion.h>

using namespace libebml;

int main ()
{
  FILE *f;
  f = fopen("conf.ebmltest", "wb");
  if (f == NULL)
    return 1;
  fprintf(f, "%s\n", EbmlCodeVersion.c_str());
  fclose(f);
  return 0;
}

],, no_ebml=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       AC_LANG_C
       CXXFLAGS="$ac_save_CXXFLAGS"
       LIBS="$ac_save_LIBS"
  fi

  if test "x$no_ebml" = "x" -a -f conf.ebmltest ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])     
  else
     AC_MSG_RESULT(no)
     exit 1
  fi

  AC_MSG_CHECKING(Ebml version)

  ebml_version=`cat conf.ebmltest`
  mver_ok=`sed 's;\.;\ ;g' < conf.ebmltest | (read -a mver
  if test ${mver[[0]]} -gt 0 ; then
    mver_ok=1
  elif test ${mver[[0]]} -lt 0 ; then
    mver_ok=0
  else
    if test ${mver[[1]]} -gt 5 ; then
      mver_ok=1
    elif test ${mver[[1]]} -lt 5 ; then
      mver_ok=0
    else
      if test ${mver[[2]]} -ge 1 ; then
        mver_ok=1
      else
        mver_ok=0
      fi
    fi
  fi
  echo $mver_ok )`
  if test "$mver_ok" = "1" ; then
    AC_MSG_RESULT($ebml_version ok)
  else
    AC_MSG_RESULT($ebml_version too old)
    echo '*** Your Ebml version is too old. Upgrade to at least version'
    echo '*** 0.5.1 and re-run configure.'
    exit 1
  fi

  AC_SUBST(EBML_CFLAGS)
  AC_SUBST(EBML_LIBS)
  rm -f conf.ebmltest
])

# Configure paths for libmatroska

dnl PATH_MATROSKA([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for libmatroska, and define MATROSKA_CFLAGS and MATROSKA_LIBS
dnl
AC_DEFUN(PATH_MATROSKA,
[dnl 
dnl Get the cflags and libraries
dnl
AC_ARG_WITH(matroska-prefix,[  --with-matroska-prefix=PFX    Prefix where libmatroska is installed (optional)], matroska_prefix="$withval", matroska_prefix="")
AC_ARG_WITH(matroska-include,[  --with-matroska-include=DIR   Path to where the libmatroska include files installed (optional)], matroska_include="$withval", matroska_include="")
AC_ARG_WITH(matroska-lib,[  --with-matroska-lib=DIR       Path to where the libmatroska library installed (optional)], matroska_lib="$withval", matroska_lib="")
AC_ARG_ENABLE(matroskatest, [  --disable-matroskatest        Do not try to compile and run a test Matroska program],, enable_matroskatest=yes)

  if test "x$matroska_prefix" != "x"; then
    matroska_args="$matroska_args --prefix=$matroska_prefix"
    if test "x$matroska_include" != "x"; then
      MATROSKA_CFLAGS="-I$matroska_include"
    else
      MATROSKA_CFLAGS="-I$matroska_prefix/include"
    fi
    if test "x$matroska_lib" != "x"; then
      MATROSKA_LIBS="-L$matroska_lib"
    else
      MATROSKA_LIBS="-L$matroska_prefix/lib"
    fi
  elif test "x$prefix" != "xNONE"; then
    matroska_args="$matroska_args --prefix=$prefix"
    if test "x$matroska_include" != "x"; then
      MATROSKA_CFLAGS="-I$matroska_include"
    else
      MATROSKA_CFLAGS="-I$prefix/include"
    fi
    if test "x$matroska_lib" != "x"; then
      MATROSKA_LIBS="-L$matroska_lib"
    else
      MATROSKA_LIBS="-L$prefix/lib"
    fi
  else
    if test "x$matroska_include" != "x"; then
      MATROSKA_CFLAGS="-I$matroska_include"
    fi
    if test "x$matroska_lib" != "x"; then
      MATROSKA_LIBS="-L$matroska_lib"
    else
      MATROSKA_LIBS="-L/usr/local/lib"
    fi
  fi

  MATROSKA_LIBS="$MATROSKA_LIBS -lmatroska"

  AC_MSG_CHECKING(for Matroska)
  no_matroska=""


  if test "x$enable_matroskatest" = "xyes" ; then
    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LIBS="$LIBS"
    CXXFLAGS="$CXXFLAGS $MATROSKA_CFLAGS $EBML_CFLAGS"
    LIBS="$LIBS $MATROSKA_LIBS $EBML_LIBS"
dnl
dnl Now check if the installed Matroska is sufficiently new.
dnl
      rm -f conf.matroskatest
      AC_LANG_CPLUSPLUS
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ebml/EbmlConfig.h>
#include <matroska/KaxVersion.h>

using namespace LIBMATROSKA_NAMESPACE;

int main ()
{
  FILE *f;
  f = fopen("conf.matroskatest", "wb");
  if (f == NULL)
    return 1;
  fprintf(f, "%s\n", KaxCodeVersion.c_str());
  fclose(f);
  return 0;
}

],, no_matroska=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
      AC_LANG_C
      CXXFLAGS="$ac_save_CXXFLAGS"
      LIBS="$ac_save_LIBS"
  fi

  if test "x$no_matroska" = "x" -a -f conf.matroskatest ; then
     AC_MSG_RESULT(yes)
     ifelse([$1], , :, [$1])     
  else
     AC_MSG_RESULT(no)
     exit 1
  fi

  AC_MSG_CHECKING(Matroska version)

  matroska_version=`cat conf.matroskatest`
  mver_ok=`sed 's;\.;\ ;g' < conf.matroskatest | (read -a mver
  if test ${mver[[0]]} -gt 0 ; then
    mver_ok=1
  elif test ${mver[[0]]} -lt 0 ; then
    mver_ok=0
  else
    if test ${mver[[1]]} -gt 5 ; then
      mver_ok=1
    elif test ${mver[[1]]} -lt 5 ; then
      mver_ok=0
    else
      if test ${mver[[2]]} -ge 1 ; then
        mver_ok=1
      else
        mver_ok=0
      fi
    fi
  fi
  echo $mver_ok )`
  if test "$mver_ok" = "1" ; then
    AC_MSG_RESULT($matroska_version ok)
  else
    AC_MSG_RESULT($matroska_version too old)
    echo '*** Your Matroska version is too old. Upgrade to at least version'
    echo '*** 0.5.1 and re-run configure.'
    exit 1
  fi

  AC_SUBST(MATROSKA_CFLAGS)
  AC_SUBST(MATROSKA_LIBS)
  rm -f conf.matroskatest
])

dnl
dnl Check for mingw
dnl
AC_DEFUN(PATH_MINGW,
[dnl
  AC_MSG_CHECKING(if being compiled with mingw32)
  if test "x`gcc --version | grep -i mingw`" = "x" ; then
    AC_MSG_RESULT(no)
  else
    AC_MSG_RESULT(yes)
    export MINGW="1"
    MINGW_GUIAPP=-mwindows
dnl    MINGW_LIBS=-lcharset
  fi
  AC_SUBST(MINGW_LIBS)
  AC_SUBST(MINGW_GUIAPP)
])

dnl This macros shamelessly stolen from
dnl http://gcc.gnu.org/ml/gcc-bugs/2001-06/msg01398.html.
dnl Written by Bruno Haible.

AC_DEFUN(PATH_ICONV,
[
  dnl Some systems have iconv in libc, some have it in libiconv (OSF/1 and
  dnl those with the standalone portable GNU libiconv installed).

  AC_ARG_WITH([libiconv-prefix],
[  --with-libiconv-prefix=DIR    search for libiconv in DIR/include and DIR/lib], [
    for dir in `echo "$withval" | tr : ' '`; do
      if test -d $dir/include; then CPPFLAGS="$CPPFLAGS -I$dir/include"; fi
      if test -d $dir/lib; then LDFLAGS="$LDFLAGS -L$dir/lib"; fi
    done
   ])

  AC_CACHE_CHECK(for iconv, am_cv_func_iconv, [
    am_cv_func_iconv="no, consider installing GNU libiconv"
    am_cv_lib_iconv=no
    AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
      [iconv_t cd = iconv_open("","");
       iconv(cd,NULL,NULL,NULL,NULL);
       iconv_close(cd);],
      am_cv_func_iconv=yes)
    if test "$am_cv_func_iconv" != yes; then
      am_save_LIBS="$LIBS"
      LIBS="$LIBS -liconv"
      AC_TRY_LINK([#include <stdlib.h>
#include <iconv.h>],
        [iconv_t cd = iconv_open("","");
         iconv(cd,NULL,NULL,NULL,NULL);
         iconv_close(cd);],
        am_cv_lib_iconv=yes
        am_cv_func_iconv=yes)
      LIBS="$am_save_LIBS"
    fi
  ])
  if test "$am_cv_func_iconv" = yes; then
    AC_MSG_CHECKING([for iconv declaration])
    AC_CACHE_VAL(am_cv_proto_iconv, [
      AC_TRY_COMPILE([
#include <stdlib.h>
#include <iconv.h>
extern
#ifdef __cplusplus
"C"
#endif
#if defined(__STDC__) || defined(__cplusplus)
size_t iconv (iconv_t cd, char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);
#else
size_t iconv();
#endif
], [], am_cv_proto_iconv_arg1="", am_cv_proto_iconv_arg1="const")
      am_cv_proto_iconv="extern size_t iconv (iconv_t cd, $am_cv_proto_iconv_arg1 char * *inbuf, size_t *inbytesleft, char * *outbuf, size_t *outbytesleft);"])
    am_cv_proto_iconv=`echo "[$]am_cv_proto_iconv" | tr -s ' ' | sed -e 's/( /(/'`
    AC_MSG_RESULT([$]{ac_t:-
         }[$]am_cv_proto_iconv)
dnl    AC_DEFINE_UNQUOTED(ICONV_CONST, $am_cv_proto_iconv_arg1,
dnl      [Define as const if the declaration of iconv() needs const.])
  else
    echo '*** The iconv library is needed but could not be found.'
    echo '*** Please install it and re-run configure.'
    exit 1
  fi
  ICONV_LIBS=
  if test "$am_cv_lib_iconv" = yes; then
    ICONV_LIBS="-liconv"
  fi
  AC_SUBST(ICONV_LIBS)
])

dnl PATH_WXWINDOWS([ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl Test for wxWindows, and define MATROSKA_CFLAGS and MATROSKA_LIBS
dnl
AC_DEFUN(PATH_WXWINDOWS,
[
  AC_MSG_CHECKING(for wxWindows)
  if wx-config --cxxflags > /dev/null 2>&1; then
    wxwversion=`wx-config --version`
    wxwver_ok=`echo $wxwversion | sed 's;\.;\ ;g' | (read -a mver
    if test ${mver[[0]]} -gt 2 ; then
      wxwver_ok=1
    elif test ${mver[[0]]} -lt 2 ; then
      wxwver_ok=0
    else
      if test ${mver[[1]]} -ge 4 ; then
        wxwver_ok=1
      else
        wxwver_ok=0
      fi
    fi
    echo $wxwver_ok )`

    if test "$wxwver_ok" = "1" ; then
      if test "$MINGW" = "1" ; then
        WXWINDOWS_CFLAGS=$(wx-config --cxxflags | sed 's;-I;-Ic:/cygwin;g')
        WXWINDOWS_LDFLAGS=$(wx-config --ldflags | sed 's;-L;-Lc:/cygwin;g')
        WXWINDOWS_LIBS=$(wx-config --libs | sed -e 's;-L;-Lc:/cygwin;g' -e 's; /usr/local/lib/libwx; c:/cygwin/usr/local/lib/libwx;g')
      else
        WXWINDOWS_CFLAGS=$(wx-config --cxxflags)
        WXWINDOWS_LDFLAGS=$(wx-config --ldflags)
        WXWINDOWS_LIBS=$(wx-config --libs)
      fi
      echo '#define HAVE_WXWINDOWS 1' >> config.h
      AC_MSG_RESULT($wxwversion ok)
      have_wxwindows=yes
    else
      AC_MSG_RESULT(no: version $wxwversion is too old)
    fi
  else
    echo '/* #define HAVE_WXWINDOWS 1 */' >> config.h
    AC_MSG_RESULT(no: wx-config was not found)
  fi

  AC_SUBST(WXWINDOWS_CFLAGS)
  AC_SUBST(WXWINDOWS_LDFLAGS)
  AC_SUBST(WXWINDOWS_LIBS)
])

AC_DEFUN(PATH_EXPAT,
[ AC_ARG_WITH(expat,
	      [  --with-expat=PREFIX           Path to where the Expat library is installed],
	      , with_expat_given=no)

  EXPAT_CFLAGS=
  EXPAT_LIBS=
	if test "$with_expat" != "yes" -a "$with_expat" != ""; then
		EXPAT_CFLAGS="-I$with_expat/include"
		EXPAT_LIBS="-L$with_expat/lib"
	fi
	AC_CHECK_LIB(expat, XML_ParserCreate,
		     [ EXPAT_LIBS="$EXPAT_LIBS -lexpat"
		       expat_found=yes ],
		     [ expat_found=no ],
		     "$EXPAT_LIBS")
	if test "$expat_found" = "no"; then
		AC_MSG_ERROR([Could not find the Expat library])
	fi
	expat_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $EXPAT_CFLAGS"
	AC_CHECK_HEADERS(expat.h, , expat_found=no)
	if test "$expat_found" = "no"; then
		AC_MSG_ERROR([Could not find expat.h])
	fi
	CFLAGS="$expat_save_CFLAGS"

  if test "x$expat_found" = "xno"; then
    exit 1
  fi

  AC_SUBST(EXPAT_CFLAGS)
  AC_SUBST(EXPAT_LIBS)
])

AC_DEFUN(PATH_ZLIB,
[ AC_ARG_WITH(zlib,
	      [  --with-zlib=PREFIX            Path to where the zlib library is installed],
	      , with_zlib_given=no)

  ZLIB_CFLAGS=
  ZLIB_LIBS=
  if test "$with_zlib" != "yes" -a "$with_zlib" != ""; then
    ZLIB_CFLAGS="-I$with_zlib/include"
    ZLIB_LIBS="-L$with_zlib/lib"
  fi

	AC_CHECK_LIB(z, zlibVersion,
		     [ ZLIB_LIBS="$ZLIB_LIBS -lz"
		       zlib_found=yes ],
		     [ zlib_found=no ],
		     "$ZLIB_LIBS")
  if test "$zlib_found" = "no"; then
    AC_MSG_ERROR([Could not find the zlib library])
  fi
	zlib_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $ZLIB_CFLAGS"
  AC_CHECK_HEADERS(zlib.h, , zlib_found=no)
	CFLAGS="$zlib_save_CFLAGS"

  AC_SUBST(ZLIB_CFLAGS)
  AC_SUBST(ZLIB_LIBS)
])

AC_DEFUN(PATH_LZO,
[ AC_ARG_WITH(lzo,
	      [  --with-lzo=PREFIX             Path to where the lzo library is installed],
	      , with_lzo_given=no)

  LZO_CFLAGS=
  LZO_LIBS=
  if test "$with_lzo" != "yes" -a "$with_lzo" != ""; then
    LZO_CFLAGS="-I$with_lzo/include"
    LZO_LIBS="-L$with_lzo/lib"
  fi

	AC_CHECK_LIB(lzo, lzo1x_1_compress,
		     [ LZO_LIBS="$LZO_LIBS -llzo"
		       lzo_found=yes ],
		     [ lzo_found=no ],
		     "$LZO_LIBS")
  if test "$lzo_found" = "no"; then
    AC_MSG_ERROR([Could not find the lzo library])
  fi
	lzo_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $LZO_CFLAGS"
  AC_CHECK_HEADERS(lzo1x.h, , lzo_found=no)
	CFLAGS="$lzo_save_CFLAGS"

  AC_SUBST(LZO_CFLAGS)
  AC_SUBST(LZO_LIBS)
])

AC_DEFUN(PATH_BZ2,
[ AC_ARG_WITH(bz2,
	      [  --with-bz2=PREFIX             Path to where the bz2 library is installed],
	      , with_bz2_given=no)

  BZ2_CFLAGS=
  BZ2_LIBS=
  if test "$with_bz2" != "yes" -a "$with_bz2" != ""; then
    BZ2_CFLAGS="-I$with_bz2/include"
    BZ2_LIBS="-L$with_bz2/lib"
  fi

	AC_CHECK_LIB(bz2, BZ2_bzCompress,
		     [ BZ2_LIBS="$BZ2_LIBS -lbz2"
		       bz2_found=yes ],
		     [ bz2_found=no ],
		     "$BZ2_LIBS")
  if test "$bz2_found" = "no"; then
    AC_MSG_ERROR([Could not find the bz2 library])
  fi
	bz2_save_CFLAGS="$CFLAGS"
	CFLAGS="$CFLAGS $BZ2_CFLAGS"
  AC_CHECK_HEADERS(bzlib.h, , bz2_found=no)
	CFLAGS="$bz2_save_CFLAGS"

  AC_SUBST(BZ2_CFLAGS)
  AC_SUBST(BZ2_LIBS)
])

