dnl
dnl Check for libogg
dnl
  AC_CHECK_LIB(ogg, ogg_sync_init,
               [ OGG_LIBS="-logg"
                 ogg_found=yes ],
               [ ogg_found=no ],)
  if test "$ogg_found" = "no"; then
    AC_MSG_ERROR([Could not find the Ogg library])
  fi
  AC_CHECK_HEADERS(ogg/ogg.h, , ogg_found=no)
  if test "$ogg_found" = "no"; then
    AC_MSG_ERROR([Could not find the Ogg header files])
  fi

AC_SUBST(OGG_LIBS)
