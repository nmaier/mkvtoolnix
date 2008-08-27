dnl Stolen from VideoLAN Client, http://www.videolan.org/
dnl
dnl  Endianness check, AC_C_BIGENDIAN doesn't work if we are cross-compiling
dnl
dnl  We give the user the opportunity to specify
dnl  --with-words=big or --with-words=little ; otherwise, try to guess
dnl
AC_ARG_WITH(words,
  AC_HELP_STRING([--with-words=endianness],[set endianness (big or little)]))
  case "${with_words}" in
    big)
      ac_cv_c_bigendian=yes
      ;;
    little)
      ac_cv_c_bigendian=no
      ;;
    *)
      dnl  Try to guess endianness by matching patterns on a compiled
      dnl  binary, by looking for an ASCII or EBCDIC string
      AC_CACHE_CHECK([whether the byte order is big-endian],
        [ac_cv_c_bigendian],
        [ac_cv_c_bigendian="unknown"
        [cat >conftest.c <<EOF
        short am[] = { 0x4249, 0x4765, 0x6e44, 0x6961, 0x6e53, 0x7953, 0 };
        short ai[] = { 0x694c, 0x5454, 0x656c, 0x6e45, 0x6944, 0x6e61, 0 };
        void _a(void) { char*s = (char*)am; s = (char *)ai; }
        short ei[] = { 0x89D3, 0xe3e3, 0x8593, 0x95c5, 0x89c4, 0x9581, 0 };
        short em[] = { 0xc2c9, 0xc785, 0x95c4, 0x8981, 0x95e2, 0xa8e2, 0 };
        void _e(void) { char*s = (char*)em; s = (char*)ei; }
        int main(void) { _a(); _e(); return 0; }
EOF
        ]
        if test -f conftest.c
        then
          if ${CC-cc} -c conftest.c -o conftest.o >>config.log 2>&1 \
              && test -f conftest.o
          then
            if test "`strings conftest.o | grep BIGenDianSyS`"
            then
              ac_cv_c_bigendian="yes"
            fi
            if test "`strings conftest.o | grep LiTTleEnDian`"
            then
              ac_cv_c_bigendian="no"
            fi
          fi
        fi
      ])
      if test "${ac_cv_c_bigendian}" = "unknown"
      then
        AC_MSG_ERROR([Could not guess endianness, please use --with-words])
      fi
      ;;
  esac
dnl  Now we know what to use for endianness, just put it in the header
if test "${ac_cv_c_bigendian}" = "yes"
then
  AC_DEFINE(WORDS_BIGENDIAN, 1, big endian system)
fi
