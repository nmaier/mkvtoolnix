dnl
dnl Test for libebml, and define EBML_CFLAGS and EBML_LIBS
dnl
  ebml_ver_req_major=1
  ebml_ver_req_minor=2
  ebml_ver_req_micro=0

  AC_CACHE_CHECK([for libEBML headers version >= ${ebml_ver_req_major}.${ebml_ver_req_minor}.${ebml_ver_req_micro}],
    [ac_cv_ebml_found],[

    EBML_LIBS="-lebml"
    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LIBS="$LIBS"
    LIBS="$LIBS $EBML_LIBS"
    AC_LANG_PUSH(C++)
    AC_TRY_COMPILE([
#include <ebml/EbmlVersion.h>
#include <ebml/EbmlDummy.h>

using namespace libebml;

#if LIBEBML_VERSION < ((${ebml_ver_req_major} << 16) + (${ebml_ver_req_minor} << 8) + ${ebml_ver_req_micro})
# error libebml is too old
#endif
      ],
      [],
      ac_cv_ebml_found=yes,
      ac_cv_ebml_found=no)

    if test "${ac_cv_ebml_found}" = "no" ; then
      EBML_CFLAGS="-I/usr/local/include"
      EBML_LIBS="-L/usr/local/lib $EBML_LIBS"
      CXXFLAGS="-I/usr/local/include $CXXFLAGS"
      LIBS="-L/usr/local/lib $LIBS"
      AC_TRY_COMPILE([
#include <ebml/EbmlVersion.h>
#include <ebml/EbmlDummy.h>

using namespace libebml;

#if LIBEBML_VERSION < ((${ebml_ver_req_major} << 16) + (${ebml_ver_req_minor} << 8) + ${ebml_ver_req_micro})
# error libebml is too old
#endif
        ],
        [],
        ac_cv_ebml_found=yes,
        ac_cv_ebml_found=no)
    fi

    AC_LANG_POP
    CXXFLAGS="$ac_save_CXXFLAGS"
    LIBS="$ac_save_LIBS"

    if test x"${ac_cv_ebml_found}" != "xyes" ; then
      ac_cv_ebml_found=internal
    fi
  ])

  if test x"${ac_cv_ebml_found}" = "xinternal" ; then
    EBML_CFLAGS="-Ilib/libebml"
    EBML_LIBS="-Llib/libebml/src -lebml"

  else
dnl
dnl Test if libebml has to be compiled with -DEBML_DLL on Windows.
dnl
    ebml_check_msg_nodll="yes, without -DEBML_DLL"
    ebml_check_msg_dll="yes, with -DEBML_DLL"

    AC_CACHE_CHECK([if linking against libEBML works and if it requires -DEBML_DLL],
      [ac_cv_ebml_dll],[
        AC_LANG_PUSH(C++)
        ac_save_CXXFLAGS="$CXXFLAGS"
        ac_save_LIBS="$LIBS"
        CXXFLAGS="$CXXFLAGS $EBML_CFLAGS"
        LIBS="$LIBS $EBML_LIBS"
        AC_TRY_LINK([
#include <ebml/EbmlVersion.h>
#include <ebml/EbmlDummy.h>

using namespace libebml;
          ],
          [EbmlDummy d;],
          ac_cv_ebml_dll="${ebml_check_msg_nodll}",
          ac_cv_ebml_dll="not found")

        if test x"${ac_cv_mingw32}" = "xyes" ; then
          if test x"${ac_cv_ebml_dll}" != "x${ebml_check_msg_nodll}" ; then
            CXXFLAGS="$CXXFLAGS -DEBML_DLL"
            AC_TRY_LINK([
#include <ebml/EbmlVersion.h>
#include <ebml/EbmlDummy.h>

using namespace libebml;
              ],
              [EbmlDummy d;],
              ac_cv_ebml_dll="${ebml_check_msg_dll}")
          fi
        fi
        AC_LANG_POP
        CXXFLAGS="${ac_save_CXXFLAGS}"
        LIBS="${ac_save_LIBS}"
      ])

    if test x"${ac_cv_ebml_dll}" != "x${ebml_check_msg_dll}" -a x"${ac_cv_ebml_dll}" != "x${ebml_check_msg_nodll}" ; then
      echo '*** The libEBML library was not found.'
      exit 1
    fi
  fi

  if test x"${ac_cv_ebml_dll}" = "x${ebml_check_msg_dll}" ; then
    EBML_CFLAGS="$EBML_CFLAGS -DEBML_DLL"
  fi

AC_SUBST(EBML_CFLAGS)
AC_SUBST(EBML_LIBS)
