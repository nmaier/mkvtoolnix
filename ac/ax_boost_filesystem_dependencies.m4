AC_DEFUN([AX_BOOST_FILESYSTEM_DEPENDENCIES],[
  AC_CACHE_CHECK([if Boost::Filesystem requires Boost::System],
                 [ax_cv_boost_dependencies_system],[
    CPPFLAGS_SAVED="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
    export CPPFLAGS

    LDFLAGS_SAVED="$LDFLAGS"
    LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
    export LDFLAGS

    AC_LANG_PUSH(C++)
    AC_LINK_IFELSE([
      AC_LANG_PROGRAM([
          #include <boost/filesystem.hpp>
        ],[
          boost::filesystem::path p1("/etc/hosts");
          int i = p1.stem().length();
        ])],
      [ax_cv_boost_dependencies_system=no],
      [ax_cv_boost_dependencies_system=yes])
    AC_LANG_POP()

    CPPFLAGS="$CPPFLAGS_SAVED"
    LDFLAGS="$LDFLAGS_SAVED"
  ])
])
