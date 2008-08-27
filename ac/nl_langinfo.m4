dnl
dnl Check for nl_langinfo or at least locale_charset
dnl
if test x"$MINGW" != "x1" ; then
  AC_MSG_CHECKING(for nl_langinfo)
  AC_CACHE_VAL(ac_cv_has_nl_langinfo,[
    AC_TRY_COMPILE([
#include <langinfo.h>
      ],
      [nl_langinfo(CODESET);],
      ac_cv_has_nl_langinfo=yes,
      ac_cv_has_nl_langinfo=no)
    ])
  AC_MSG_RESULT($ac_cv_has_nl_langinfo)
  if test x"$ac_cv_has_nl_langinfo" = "xyes" ; then
    AC_DEFINE(HAVE_NL_LANGINFO, 1, [nl_langinfo is available])
  else
    AC_MSG_CHECKING(for locale_charset)
    AC_CACHE_VAL(ac_cv_has_locale_charset,[
      AC_TRY_COMPILE([
#include <libcharset.h>
        ],
        [locale_charset();],
        ac_cv_has_locale_charset=yes,
        ac_cv_has_locale_charset=no)
      ])
    AC_MSG_RESULT($ac_cv_has_locale_charset)
    if test x"$ac_cv_has_locale_charset" = "xyes" ; then
      AC_DEFINE(HAVE_LOCALE_CHARSET, 1, [locale_charset is available])
    else
      echo '*** Your system has neither nl_langinfo nor locale_charset.'
      echo '*** Please install libcharset which is part of libiconv'
      echo '*** available at http://www.gnu.org/software/libiconv/'
      exit 1
    fi
  fi
fi
