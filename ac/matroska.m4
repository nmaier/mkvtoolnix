dnl
dnl Test for libmatroska, and define MATROSKA_CFLAGS and MATROSKA_LIBS
dnl
  kax_ver_req_major=0
  kax_ver_req_minor=8
  kax_ver_req_micro=1

  AC_CACHE_CHECK([for libMatroska headers version >= ${kax_ver_req_major}.${kax_ver_req_minor}.${kax_ver_req_micro}],
    [ac_cv_matroska_found],
    [

    MATROSKA_LIBS="-lmatroska"
    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LIBS="$LIBS"
    CXXFLAGS="$CFLAGS $MATROSKA_CFLAGS"
    LIBS="$LIBS $MATROSKA_LIBS $EBML_LIBS"
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
      MATROSKA_LIBS="-L/usr/local/lib $MATROSKA_LIBS"
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
  ])

  if test x"${ac_cv_matroska_found}" != "xyes" ; then
    echo '*** Your libMatroska version is too old. Upgrade to at least version'
    echo '*** '${kax_ver_req_major}.${kax_ver_req_minor}.${kax_ver_req_micro}' and re-run configure.'
    exit 1
  fi

  matroska_check_msg_nodll="yes, without -MATROSKA_DLL"
  matroska_check_msg_dll="yes, with -DMATROSKA_DLL"

  AC_CACHE_CHECK([if linking against libMatroska works and if it requires -DMATROSKA_DLL],
    [ac_cv_matroska_dll],[
      AC_LANG_PUSH(C++)
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_LIBS="$LIBS"
      CXXFLAGS="$CXXFLAGS $EBML_CFLAGS $MATROSKA_CFLAGS"
      LIBS="$LIBS $MATROSKA_LIBS $EBML_LIBS"
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

  if test x"${ac_cv_ebml_dll}" != "xyes" -o x"${ac_cv_matroska_dll}" != "xyes" ; then
    LIBMTXCOMMONDLL=0
  fi

AC_SUBST(MATROSKA_CFLAGS)
AC_SUBST(MATROSKA_LIBS)
