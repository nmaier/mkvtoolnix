AC_CACHE_CHECK(
  [gcc version],
  [ac_cv_gcc_version],
  [ac_cv_gcc_version=`$ac_cv_prog_CXX -dumpversion | sed -e 's/[[^0-9\.]]//g'`])
