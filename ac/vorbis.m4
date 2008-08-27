dnl
dnl Check for libvorbis
dnl
  AC_CHECK_LIB(vorbis, vorbis_synthesis_init,
               [ VORBIS_LIBS="-lvorbis -lm"
                 vorbis_found=yes ],
               [ vorbis_found=no ],
               $OGG_LIBS -lm)
  if test "$vorbis_found" = "no"; then
    AC_MSG_ERROR([Could not find the Vorbis library])
  fi
  AC_CHECK_HEADERS(vorbis/codec.h, , vorbis_found=no)
  if test "$vorbis_found" = "no"; then
    AC_MSG_ERROR([Could not find the Vorbis header files])
  fi

AC_SUBST(VORBIS_LIBS)
