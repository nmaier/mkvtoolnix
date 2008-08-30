AX_BOOST_BASE()
AX_BOOST_REGEX()

if test x"$ax_cv_boost_regex" != "xyes"; then
  AC_MSG_ERROR(The Boost regex library was not found.)
fi
