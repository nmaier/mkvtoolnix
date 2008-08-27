check_version() {
  check_ver_req="$1"
  check_ver_real="$2"

  set - `echo $check_ver_req 0 0 0 | sed 's/\./\ /g'`
  check_ver_major=$1
  check_ver_minor=$2
  check_ver_micro=$3

  set - `echo $check_ver_real 0 0 0 | sed 's/\./\ /g'`
  check_ver_ok=0

  if test "x$1" = "x" -o $1 -lt ${check_ver_major} ; then
    check_ver_ok=0
  elif test $1 -gt ${check_ver_major} ; then
    check_ver_ok=1
  elif test "x$2" = "x" -o $2 -lt ${check_ver_minor} ; then
    check_ver_ok=0
  elif test $2 -gt ${check_ver_minor} ; then
    check_ver_ok=1
  elif test "x$3" = "x" -o $3 -lt ${check_ver_micro} ; then
    check_ver_ok=0
  else
    check_ver_ok=1
  fi

  test $check_ver_ok = 1
}
