# boost's headers must be present.
AX_BOOST_BASE([1.46.0])

# boost::filesystem must be present.
AX_BOOST_FILESYSTEM()

if test x"$ax_cv_boost_filesystem" != "xyes"; then
  AC_MSG_ERROR(The Boost Filesystem Library was not found.)
fi

# boost::regex must be present.
AX_BOOST_REGEX()

if test x"$ax_cv_boost_regex" != "xyes"; then
  AC_MSG_ERROR(The Boost Regex Library was not found.)
fi

# boost::system must be present.
AX_BOOST_SYSTEM()

if test x"$ax_cv_boost_system" != "xyes"; then
  AC_MSG_ERROR(The Boost System Library was not found.)
fi

AX_BOOST_CHECK_HEADERS([boost/property_tree/ptree.hpp],,[
  AC_MSG_ERROR([Boost's property tree library is required but wasn't found])
])
