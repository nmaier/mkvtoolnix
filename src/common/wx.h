/*
   wxcommon.h

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for wxWindows

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_WXCOMMON_H
#define __MTX_COMMON_WXCOMMON_H

#include "common/os.h"

#include <boost/format.hpp>
#include <string>

#include <ebml/EbmlString.h>

#include <wx/version.h>

#if !defined(wxUSE_UNICODE) || !wxUSE_UNICODE
# error wxWidgets was not compiled with Unicode support.
#endif

using namespace libebml;

inline wxString
wxU(const char *s) {
  return wxString(s, wxConvUTF8);
}

inline wxString
wxU(const std::string &s) {
  return wxString(s.c_str(), wxConvUTF8);
}

inline wxString
wxU(const boost::format &s) {
  return wxU(s.str());
}

inline wxString
wxU(const boost::wformat &s) {
  return wxString(s.str().c_str());
}

inline wxString
wxU(const EbmlString &s) {
  return wxU(static_cast<const std::string &>(s));
}

inline wxString
wxU(EbmlString *s) {
  if (NULL == s)
    return wxEmptyString;
  return wxU(static_cast<const std::string &>(*s));
}

inline const wxString &
wxU(const wxString &s) {
  return s;
}

#define wxUCS(s) wxU(s).c_str()
#define wxMB(s)  ((const char *)(s).mb_str(wxConvUTF8))

/* i18n stuff */
#if !defined Z
# define Z(s) wxU(Y(s))
# define NZ(s_singular, s_plural, count) wxU(NY(s))
#endif

// Use wxComboBox on non-Windows builds with wxWidgets 2.8.0 and newer
// because GTK's combo box has serious problems (see bug 339).
// wxWidgets 2.9.x's default wxComboBox is good, though.
#if !defined(wxMTX_COMBOBOX_TYPE)
# if !defined(SYS_WINDOWS) && !defined(SYS_APPLE) && defined(HAVE_WXBITMAPCOMBOBOX) && HAVE_WXBITMAPCOMBOBOX && !wxCHECK_VERSION(2, 9, 0)
#  define USE_WXBITMAPCOMBOBOX
#  define wxMTX_COMBOBOX_TYPE wxBitmapComboBox
#  include <wx/bmpcbox.h>

# else

#  define wxMTX_COMBOBOX_TYPE wxComboBox
#  include <wx/combobox.h>

# endif
#endif

#endif /* __WXCOMMON_H */
