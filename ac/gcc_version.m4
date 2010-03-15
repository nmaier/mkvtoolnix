AC_CACHE_CHECK(
  [gcc version],
  [ac_cv_gcc_version],
  [ac_cv_gcc_version=`$CXX -dumpversion | sed -e 's/[[^0-9\.]].*//g'`])
