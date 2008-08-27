dnl
dnl Check for libexpat
dnl
  AC_CHECK_LIB(expat, XML_ParserCreate,
               [ EXPAT_LIBS="-lexpat"
                 expat_found=yes ],
               [ expat_found=no ],)
  if test "$expat_found" = "no"; then
    AC_MSG_ERROR([Could not find the Expat library])
  fi
  AC_CHECK_HEADERS(expat.h, , expat_found=no)
  if test "$expat_found" = "no"; then
    AC_MSG_ERROR([Could not find expat.h])
  fi

AC_SUBST(EXPAT_CFLAGS)
AC_SUBST(EXPAT_LIBS)
