AX_BOOST_BASE()
AX_BOOST_REGEX()

if test x"$ax_cv_boost_regex" != "xyes"; then
  AC_MSG_ERROR(The Boost regex library was not found.)
fi

boost_foreach_dir=lib/boost/foreach
AX_BOOST_FOREACH($boost_foreach_dir)

if test x"$ax_cv_boost_foreach" = "xno"; then
  AC_MSG_ERROR([The Boost foreach library was not found, and the included version does not work. Install Boost v1.34.0 or later.])
fi

if test x"$ax_cv_boost_foreach" = "xincluded"; then
  EXTRA_CFLAGS="$EXTRA_CFLAGS -I$boost_foreach_dir"
fi
