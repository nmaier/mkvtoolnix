dnl
dnl Check for wxWidgets
dnl
  AC_ARG_ENABLE([gui],
    AC_HELP_STRING([--enable-gui],[compile mkvinfo's GUI and mmg (yes)]))

  AC_ARG_ENABLE([wxwidgets],
    AC_HELP_STRING([--enable-wxwidgets],[compile the wxWidgets version of the GUIs (yes)]))

  wxw_min_ver=2.6.0

  if test '(' x"$enable_wxwidgets" = xyes -o x"$enable_wxwidgets" = x ')' -a \
    '(' x"$enable_gui" = xyes -o x"$enable_gui" = x ')'; then
    AC_ARG_WITH(wx_config,
      AC_HELP_STRING([--with-wx-config=prog],
        [use prog instead of looking for wx-config]),
      [ WX_CONFIG="$with_wx_config" ],)
    AC_PATH_PROG(WX_CONFIG, wx-config, no, $PATH:/usr/local/bin)
    if test x"$WX_CONFIG" != "xno" ; then
      AC_MSG_CHECKING(for wxWidgets $wxw_min_ver or newer)

      wxwversion=`$WX_CONFIG --version`
      if check_version $wxw_min_ver $wxwversion ; then
        WXWIDGETS_CFLAGS=`$WX_CONFIG --cxxflags`
        WXWIDGETS_LIBS=`$WX_CONFIG --libs | \
          sed -e 's/-Wl,--subsystem,windows//' -e 's/-mwindows//'`
        AC_CACHE_VAL(am_cv_wx_compilation, [
          AC_LANG_PUSH(C++)
          ac_save_CXXFLAGS="$CXXFLAGS"
          ac_save_LIBS="$LIBS"
          CXXFLAGS="$CXXFLAGS $WXWIDGETS_CFLAGS"
          LIBS="$LDFLAGS $WXWIDGETS_LIBS"
          AC_TRY_LINK([
#include <wx/dnd.h>
#include <wx/treectrl.h>
], [
wxDragResult result = wxDragError;
wxTreeItemId id;
], [ am_cv_wx_compilation=1 ], [ am_cv_wx_compilation=0 ])
          AC_LANG_POP()
          CXXFLAGS="$ac_save_CXXFLAGS"
          LIBS="$ac_save_LIBS"
        ])
        if test x"$am_cv_wx_compilation" = x1; then
          if test "x$MINGW" = "x1" ; then
            WXWIDGETS_INCLUDES=""
            set - `echo $WXWIDGETS_CFLAGS`
            while test "x$1" != "x" ; do
              case "$1" in
                -I*)
                  WXWIDGETS_INCLUDES="$WXWIDGETS_INCLUDES $1"
                  ;;
              esac
              shift
            done
          fi
          AC_DEFINE(HAVE_WXWIDGETS, 1, [Define if wxWindows is present])
          AC_MSG_RESULT($wxwversion ok)
          have_wxwindows=yes
          USE_WXWIDGETS=yes
          opt_features_yes="$opt_features_yes\n   * GUIs (wxWidgets version)"
        else
          AC_MSG_RESULT(no: test program could not be compiled)
        fi
      else
        AC_MSG_RESULT(no: version $wxwversion is too old)
      fi
    else
      AC_MSG_RESULT(no: wx-config was not found)
    fi
  else
    echo '*** Not checking for wxWidgets: disabled by user request'
  fi

  if test x"$have_wxwindows" != "xyes" ; then
    opt_features_no="$opt_features_no\n   * GUIs (wxWidgets version)"
  else
    AC_MSG_CHECKING(for wxWidgets class wxBitmapComboBox)
    AC_CACHE_VAL(am_cv_wx_bitmapcombobox, [
      AC_LANG_PUSH(C++)
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_LIBS="$LIBS"
      CXXFLAGS="$CXXFLAGS $WXWIDGETS_CFLAGS"
      LIBS="$LDFLAGS $WXWIDGETS_LIBS"
      AC_TRY_COMPILE([
#include <wx/bmpcbox.h>
], [
wxBitmapComboBox bitmap_combobox(NULL, -1);
], [ am_cv_wx_bitmapcombobox=1 ], [ am_cv_wx_bitmapcombobox=0 ])
      AC_LANG_POP()
      CXXFLAGS="$ac_save_CXXFLAGS"
      LIBS="$ac_save_LIBS"
    ])

    if test x"$am_cv_wx_bitmapcombobox" = "x1" ; then
      AC_DEFINE(HAVE_WXBITMAPCOMBOBOX, 1, [Define if the wxWindows class wxBitmapComboBox is present])
      AC_MSG_RESULT(yes)
    else
      AC_MSG_RESULT(no)
    fi
  fi

AC_SUBST(WXWIDGETS_CFLAGS)
AC_SUBST(WXWIDGETS_INCLUDES)
AC_SUBST(WXWIDGETS_LIBS)
AC_SUBST(USE_WXWIDGETS)
