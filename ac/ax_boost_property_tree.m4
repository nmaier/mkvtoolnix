AC_DEFUN([AX_BOOST_PROPERTY_TREE_TRY_COMPILE],[
  result="$1"
  AC_LANG_PUSH(C++)
  AC_TRY_COMPILE([
      #include <boost/property_tree/ptree.hpp>
    ],[
      boost::property_tree::ptree pt;
    ],
    ax_cv_boost_property_tree=$result,
    ax_cv_boost_property_tree=no)
  AC_LANG_POP()
])

AC_DEFUN([AX_BOOST_PROPERTY_TREE],[
  included_boost_dir="$1"

  AC_CACHE_CHECK([which boost/property_tree/ptree.hpp to use],
                 [ax_cv_boost_property_tree],[
    CPPFLAGS_SAVED="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
    export CPPFLAGS

    LDFLAGS_SAVED="$LDFLAGS"
    LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
    export LDFLAGS

    AX_BOOST_PROPERTY_TREE_TRY_COMPILE(system)

    if test x"$ax_cv_boost_property_tree" = "xno"; then
      CPPFLAGS="$CPPFLAGS -I$included_boost_dir"
      export CPPFLAGS

      AX_BOOST_PROPERTY_TREE_TRY_COMPILE(included)
    fi

    CPPFLAGS="$CPPFLAGS_SAVED"
    LDFLAGS="$LDFLAGS_SAVED"
  ])
])
