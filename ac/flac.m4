dnl
dnl Check for libFLAC
dnl

  dnl FLAC 1.2.1 with mingw needs the winsock library.
  flac_winsock=
  if test "x$MINGW" = "x1" ; then
    flac_winsock="-lwsock32"
  fi

  AC_ARG_WITH([flac],
              AC_HELP_STRING([--without-flac], [do not build with flac support]),
              [ with_flac=${withval} ], [ with_flac=yes ])

  if test "$with_flac" != "no"; then
    AC_CHECK_LIB(FLAC, FLAC__stream_decoder_new,
                 [ FLAC_LIBS="-lFLAC $OGG_LIBS -lm $flac_winsock"
                   flac_found=yes ],
                 [ flac_found=no ],
                 $OGG_LIBS -lm $flac_winsock)
  else
    flac_found=no
  fi
  if test "$flac_found" = "yes"; then
    AC_CHECK_MEMBER(FLAC__StreamMetadata_StreamInfo.sample_rate, ,
                    [ flac_found=no ],
                    [ #include <FLAC/format.h>
                    ])
  fi
  if test x"$flac_found" = xyes ; then
    AC_CHECK_LIB(FLAC, FLAC__stream_decoder_skip_single_frame,
                 [ flac_decoder_skip_found=yes ],
                 [ flac_decoder_skip_found=no ],
                 $FLAC_LIBS)
    if test x"$flac_decoder_skip_found" = xyes; then
      opt_features_yes="$opt_features_yes\n   * FLAC audio"
      AC_DEFINE(HAVE_FLAC_DECODER_SKIP, [1], [Define if FLAC__stream_decoder_skip_single_frame exists])
      AC_DEFINE(HAVE_FLAC_FORMAT_H, [1], [Define if the FLAC headers are present])
    else
      FLAC_LIBS=""
      opt_features_no="$opt_features_no\n   * FLAC audio (version too old)"
    fi
  else
    FLAC_LIBS=""
    opt_features_no="$opt_features_no\n   * FLAC audio"
  fi

AC_SUBST(FLAC_LIBS)
