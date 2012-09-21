dnl
dnl Test for libmatroska, and define MATROSKA_CFLAGS and MATROSKA_LDFLAGS
dnl
  kax_ver_req_major=1
  kax_ver_req_minor=3
  kax_ver_req_micro=0

  AC_CACHE_CHECK([for libMatroska headers version >= ${kax_ver_req_major}.${kax_ver_req_minor}.${kax_ver_req_micro}],
    [ac_cv_matroska_found],
    [

    if test x"${ac_cv_ebml_found}" = "xyes" ; then
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_LIBS="$LIBS"
      CXXFLAGS="$CFLAGS $MATROSKA_CFLAGS"
      LIBS="$LIBS -lmatroska -lebml"
      rm -f conf.matroskatest
      AC_LANG_PUSH(C++)
      AC_TRY_COMPILE([
#include <ebml/EbmlConfig.h>
#include <matroska/KaxVersion.h>
#include <matroska/KaxTracks.h>

using namespace libmatroska;

#if LIBMATROSKA_VERSION < ((${kax_ver_req_major} << 16) + (${kax_ver_req_minor} << 8) + ${kax_ver_req_micro})
# error libmatroska is too old
#endif
        ],
        [],
        ac_cv_matroska_found=yes,
        ac_cv_matroska_found=no)

      if test x"${ac_cv_matroska_found}" != "xyes" ; then
        MATROSKA_CFLAGS="-I/usr/local/include"
        MATROSKA_LDFLAGS="-L/usr/local/lib"
        CXXFLAGS="-I/usr/local/include $CXXFLAGS"
        LIBS="-L/usr/local/lib $LIBS"
        AC_TRY_COMPILE([
#include <ebml/EbmlConfig.h>
#include <matroska/KaxVersion.h>
#include <matroska/KaxTracks.h>

using namespace libmatroska;

#if LIBMATROSKA_VERSION < ((${kax_ver_req_major} << 16) + (${kax_ver_req_minor} << 8) + ${kax_ver_req_micro})
# error libmatroska is too old
#endif
          ],
          [],
          ac_cv_matroska_found=yes,
          ac_cv_matroska_found=no)
      fi

      AC_LANG_POP
      CXXFLAGS="$ac_save_CXXFLAGS"
      LIBS="$ac_save_LIBS"
    fi

    if test x"${ac_cv_matroska_found}" != "xyes" ; then
      ac_cv_matroska_found=internal
    fi
  ])

  if test x"${ac_cv_matroska_found}" = "xinternal" -o x"${ac_cv_ebml_found}" = "xinternal" ; then
    if ! test -f lib/libmatroska/matroska/KaxVersion.h ; then
      echo '*** The internal version of the libMatroska library is supposed to be used,'
      echo '*** but it was not found in "lib/libmatroska". If this is a clone from the'
      echo '*** git repository then submodules have to be initialized with the'
      echo '*** following two commands:'
      echo '***'
      echo '*** git submodule init'
      echo '*** git submodule update'
      exit 1
    fi

    EBML_CFLAGS="-Ilib/libebml"
    EBML_LDFLAGS="-Llib/libebml/src"
    MATROSKA_CFLAGS="-Ilib/libmatroska"
    MATROSKA_LDFLAGS="-Llib/libmatroska/src"
    EBML_MATROSKA_INTERNAL=yes

  else
    matroska_check_msg_nodll="yes, without -MATROSKA_DLL"
    matroska_check_msg_dll="yes, with -DMATROSKA_DLL"

    AC_CACHE_CHECK([if linking against libMatroska works and if it requires -DMATROSKA_DLL],
      [ac_cv_matroska_dll],[
        AC_LANG_PUSH(C++)
        ac_save_CXXFLAGS="$CXXFLAGS"
        ac_save_LIBS="$LIBS"
        CXXFLAGS="$CXXFLAGS $EBML_CFLAGS $MATROSKA_CFLAGS"
        LIBS="$LIBS $MATROSKA_LDFLAGS -lmatroska -lebml"
        AC_TRY_LINK([
#include <matroska/KaxVersion.h>
#include <matroska/KaxSegment.h>

using namespace libmatroska;
          ],
          [KaxSegment s;],
          ac_cv_matroska_dll="${matroska_check_msg_nodll}",
          ac_cv_matroska_dll="not found")

        if test x"${ac_cv_mingw32}" = "xyes" ; then
          if test x"${ac_cv_matroska_dll}" != "x${matroska_check_msg_nodll}" ; then
            CXXFLAGS="$CXXFLAGS -DMATROSKA_DLL"
            AC_TRY_LINK([
#include <matroska/KaxVersion.h>
#include <matroska/KaxSegment.h>

using namespace libmatroska;
              ],
              [KaxSegment s;],
              ac_cv_matroska_dll="${matroska_check_msg_dll}")
          fi
        fi
        AC_LANG_POP
        CXXFLAGS="${ac_save_CXXFLAGS}"
        LIBS="${ac_save_LIBS}"
      ])

    if test x"${ac_cv_matroska_dll}" != "x${matroska_check_msg_nodll}" -a x"${ac_cv_matroska_dll}" != "x${matroska_check_msg_dll}" ; then
      echo '*** The libMatroska library was not found.'
      exit 1
    fi

    if test x"${ac_cv_matroska_dll}" = "x${matroska_check_msg_dll}" ; then
      MATROSKA_CFLAGS="$MATROSKA_CFLAGS -DMATROSKA_DLL"
    fi
  fi

AC_SUBST(MATROSKA_CFLAGS)
AC_SUBST(MATROSKA_LDFLAGS)
AC_SUBST(EBML_MATROSKA_INTERNAL)
