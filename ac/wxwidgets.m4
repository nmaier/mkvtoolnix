dnl
dnl Check for wxWidgets
dnl
  AC_ARG_ENABLE([gui],
    AC_HELP_STRING([--enable-gui],[compile mkvinfo's GUI and mmg (yes)]))

  AC_ARG_ENABLE([wxwidgets],
    AC_HELP_STRING([--enable-wxwidgets],[compile the wxWidgets version of the GUIs (yes)]))

  wxw_min_ver=2.8.0
#  wxw_min_bad_ver=2.9.0
  continue_wx_check=1

  if ! test '(' x"$enable_wxwidgets" = xyes -o x"$enable_wxwidgets" = x ')' -a '(' x"$enable_gui" = xyes -o x"$enable_gui" = x ')'; then
    echo '*** Not checking for wxWidgets: disabled by user request'
    continue_wx_check=0
  fi

  if test x"$continue_wx_check" = x1 ; then
    AC_ARG_WITH(wx_config,
      AC_HELP_STRING([--with-wx-config=prog],[use prog instead of looking for wx-config]),
      [ WX_CONFIG="$with_wx_config" ],)
    AC_PATH_PROG(WX_CONFIG, wx-config, no, $PATH:/usr/local/bin)
    if test x"$WX_CONFIG" = "xno" ; then
      echo "*** Not checking for wxWidgets: wx-config not found"
      continue_wx_check=0
    elif test ! -x "$WX_CONFIG" ; then
      echo "*** Not checking for wxWidgets: file '"$WX_CONFIG"' not executable"
      continue_wx_check=0
    fi
  fi

  if test x"$continue_wx_check" = x1 ; then
#    AC_MSG_CHECKING(for wxWidgets $wxw_min_ver or newer and older than $wxw_min_bad_ver)
    AC_MSG_CHECKING(for wxWidgets $wxw_min_ver or newer)

    wxwversion=`$WX_CONFIG --version`
    if ! check_version $wxw_min_ver $wxwversion ; then
      AC_MSG_RESULT(no: version $wxwversion is too old)
      continue_wx_check=0
    fi

#    if check_version $wxw_min_bad_ver $wxwversion ; then
#      AC_MSG_RESULT(no: there are known problems with mkvtoolnix and v$wxwversion)
#      continue_wx_check=0
#    fi
  fi

  if test x"$continue_wx_check" = x1 ; then
    WXWIDGETS_CFLAGS=`$WX_CONFIG --cxxflags`
    WXWIDGETS_LIBS=`$WX_CONFIG --libs std,richtext | \
      sed -e 's/-Wl,--subsystem,windows//' -e 's/-mwindows//'`
    AC_CACHE_VAL(ac_cv_wx_compilation, [
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
], [ ac_cv_wx_compilation=1 ], [ ac_cv_wx_compilation=0 ])
      AC_LANG_POP()
      CXXFLAGS="$ac_save_CXXFLAGS"
      LIBS="$ac_save_LIBS"
    ])

    if test x"$ac_cv_wx_compilation" != x1; then
      AC_MSG_RESULT(no: test program could not be compiled)
      continue_wx_check=0
    fi
  fi

  if test x"$continue_wx_check" = x1 ; then
    WXWIDGETS_CFLAGS=`$WX_CONFIG --cxxflags`
    WXWIDGETS_LIBS=`$WX_CONFIG --libs std,richtext | sed -e 's/-Wl,--subsystem,windows//' -e 's/-mwindows//'`
    AC_CACHE_VAL(ac_cv_wx_unicode, [
      AC_LANG_PUSH(C++)
      ac_save_CXXFLAGS="$CXXFLAGS"
      ac_save_LIBS="$LIBS"
      CXXFLAGS="$CXXFLAGS $WXWIDGETS_CFLAGS"
      LIBS="$LDFLAGS $WXWIDGETS_LIBS"
      AC_TRY_COMPILE([
#include <wx/app.h>
], [
#if defined(wxUSE_UNICODE) && wxUSE_UNICODE
int dummy;
#else
# error wxWidgets compiled without Unicode support
#endif
], [ ac_cv_wx_unicode=1 ], [ ac_cv_wx_unicode=0 ])
      AC_LANG_POP()
      CXXFLAGS="$ac_save_CXXFLAGS"
      LIBS="$ac_save_LIBS"
    ])

    if test x"$ac_cv_wx_unicode" != x1; then
      AC_MSG_RESULT(no: wxWidgets was compiled without Unicode support)
      continue_wx_check=0
    fi
  fi

  if test x"$continue_wx_check" = x1 ; then
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

    AC_LANG_PUSH(C++)
    ac_save_CXXFLAGS="$CXXFLAGS"
    ac_save_LIBS="$LIBS"
    CXXFLAGS="$CXXFLAGS $WXWIDGETS_CFLAGS"
    LIBS="$LDFLAGS $WXWIDGETS_LIBS"

    AC_CACHE_CHECK([for wxWidgets class wxBitmapComboBox], [ac_cv_wx_bitmapcombobox], [
      AC_TRY_COMPILE([
#include <wx/bmpcbox.h>
], [
wxBitmapComboBox bitmap_combobox(NULL, -1);
], [ ac_cv_wx_bitmapcombobox=yes ], [ ac_cv_wx_bitmapcombobox=no ])
    ])

    if test x"$ac_cv_wx_bitmapcombobox" = "xyes" ; then
      AC_DEFINE(HAVE_WXBITMAPCOMBOBOX, 1, [Define if the wxWindows class wxBitmapComboBox is present])
    fi

    AC_CACHE_CHECK([for wxMenuBar member function SetMenuLabel], [ac_cv_wx_menubar_setmenulabel], [
      AC_TRY_COMPILE([
#include <wx/string.h>
#include <wx/menu.h>
], [
wxMenuBar bar;
bar.SetMenuLabel(0, wxEmptyString);
], [ ac_cv_wx_menubar_setmenulabel=yes ], [ ac_cv_wx_menubar_setmenulabel=no ])
    ])

    if test x"$ac_cv_wx_menubar_setmenulabel" = "xyes" ; then
      AC_DEFINE(HAVE_WXMENUBAR_SETMENULABEL, 1, [Define if the wxWindows member function wxMenuBar::SetMenuLabel is present])
    fi

    AC_CACHE_CHECK([for wxMenuItem member function SetItemlabel], [ac_cv_wx_menuitem_setitemlabel], [
      AC_TRY_COMPILE([
#include <wx/string.h>
#include <wx/menuitem.h>
], [
wxMenuItem item;
item.SetItemLabel(wxEmptyString);
], [ ac_cv_wx_menuitem_setitemlabel=yes ], [ ac_cv_wx_menuitem_setitemlabel=no ])
    ])

    if test x"$ac_cv_wx_menuitem_setitemlabel" = "xyes" ; then
      AC_DEFINE(HAVE_WXMENUITEM_SETITEMLABEL, 1, [Define if the wxWindows member function wxMenuItem::SetItemlabel is present])
    fi

    AC_LANG_POP()
    CXXFLAGS="$ac_save_CXXFLAGS"
    LIBS="$ac_save_LIBS"

  else
    opt_features_no="$opt_features_no\n   * GUIs (wxWidgets version)"
  fi

AC_SUBST(WXWIDGETS_CFLAGS)
AC_SUBST(WXWIDGETS_INCLUDES)
AC_SUBST(WXWIDGETS_LIBS)
AC_SUBST(USE_WXWIDGETS)
