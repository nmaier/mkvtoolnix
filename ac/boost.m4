# boost's headers must be present.
AX_BOOST_BASE([1.36.0])

# boost::system can be absend for older versions. However, the test
# for boost::filesystem might fail if boost::system is not available
# with newer versions.
AX_BOOST_SYSTEM()

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

# boost::property_tree can be missing; the included version will be used in
# that case.
boost_exception_dir=lib/boost/exception
boost_property_tree_dir=lib/boost/property_tree
AX_BOOST_PROPERTY_TREE($boost_property_tree_dir, $boost_exception_dir)

if test x"$ax_cv_boost_property_tree" = "xno"; then
  AC_MSG_ERROR([The Boost property_tree library was not found, and the included version does not work. Install Boost v1.41.0 or later.])
fi

if test x"$ax_cv_boost_property_tree" = "xincluded"; then
  EXTRA_CFLAGS="-I$boost_exception_dir $EXTRA_CFLAGS -I$boost_property_tree_dir"
fi
