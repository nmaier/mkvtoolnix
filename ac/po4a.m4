TRANSLATE_PERCENT=5
PO4A_FLAGS="-M utf-8 -k $TRANSLATE_PERCENT"
PO4A_TRANSLATE_FLAGS="$PO4A_FLAGS -f docbook"
AC_PATH_PROG(PO4A, po4a)
AC_PATH_PROG(PO4A_TRANSLATE, po4a-translate)

if test "$PO4A" != "" -a "$PO4A_TRANSLATE" != ""; then
  AC_CACHE_CHECK([whether po4a-translate works],
    [ac_cv_po4a_works],
    [
      ac_cv_po4a_works=no
      $PO4A_TRANSLATE $PO4A_TRANSLATE_FLAGS -m $srcdir/doc/man/mkvmerge.xml -p $srcdir/doc/man/po4a/po/ja.po -l /dev/null
      if test "$?" = 0; then
        ac_cv_po4a_works=yes
      fi
    ])
  PO4A_WORKS=$ac_cv_po4a_works
fi

AC_SUBST(PO4A)
AC_SUBST(PO4A_TRANSLATE)
AC_SUBST(PO4A_FLAGS)
AC_SUBST(PO4A_TRANSLATE_FLAGS)
AC_SUBST(PO4A_WORKS)
