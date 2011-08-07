AC_DEFUN([AX_BOOST_ALGORITHM_STRING],[
  AC_LANG_PUSH(C++)
  AC_CHECK_HEADERS(boost/algorithm/string.hpp, , ax_cv_boost_algorithm_string=no)
  AC_LANG_POP()
])
