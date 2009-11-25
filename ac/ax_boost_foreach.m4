AC_DEFUN([AX_BOOST_FOREACH_TRY_COMPILE],[
  result="$1"
  AC_LANG_PUSH(C++)
  AC_TRY_COMPILE([
      #include <vector>
      #include <boost/foreach.hpp>
    ],[
      std::vector<int> viktor;
      BOOST_FOREACH(int i, viktor)
        i;
    ],
    ax_cv_boost_foreach=$result,
    ax_cv_boost_foreach=no)
  AC_LANG_POP()
])

AC_DEFUN([AX_BOOST_FOREACH],[
  included_boost_dir="$1"

  AC_CACHE_CHECK([which boost/foreach.hpp to use],
                 [ax_cv_boost_foreach],[
    CPPFLAGS_SAVED="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS $BOOST_CPPFLAGS"
    export CPPFLAGS

    LDFLAGS_SAVED="$LDFLAGS"
    LDFLAGS="$LDFLAGS $BOOST_LDFLAGS"
    export LDFLAGS

    AX_BOOST_FOREACH_TRY_COMPILE(system)

    if test x"$ax_cv_boost_foreach" = "xno"; then
      CPPFLAGS="$CPPFLAGS -I$included_boost_dir"
      export CPPFLAGS

      AX_BOOST_FOREACH_TRY_COMPILE(included)
    fi

    CPPFLAGS="$CPPFLAGS_SAVED"
    LDFLAGS="$LDFLAGS_SAVED"
  ])
])
