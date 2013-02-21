/*
   wxcommon.h

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for wxWindows

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_WXCOMMON_H
#define MTX_COMMON_WXCOMMON_H

#include "common/common_pch.h"

#include <ebml/EbmlString.h>

#include <wx/version.h>

#if !defined(wxUSE_UNICODE) || !wxUSE_UNICODE
# error wxWidgets was not compiled with Unicode support.
#endif

#include <wx/wx.h>
#include <wx/bitmap.h>
#include <wx/icon.h>
#include <wx/mstream.h>

#include "common/logger.h"

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
  if (!s)
    return wxEmptyString;
  return wxU(static_cast<const std::string &>(*s));
}

inline const wxString &
wxU(const wxString &s) {
  return s;
}

inline std::string
to_utf8(wxString const &source) {
  return static_cast<char const *>(source.mb_str(wxConvUTF8));
}

inline std::wstring
to_wide(wxString const &source) {
  return static_cast<wchar_t const *>(source.wc_str());
}

inline std::ostream &
operator <<(std::ostream &out,
            wxString const &s) {
  out << to_utf8(s);
  return out;
}

#define wxUCS(s) wxU(s).c_str()
#define wxMB(s)  ((const char *)(s).mb_str(wxConvUTF8))

/* i18n stuff */
#if defined(HAVE_LIBINTL_H)
# include <libintl.h>
# if !defined Z
#  define Z(s) wxU(gettext(s))
#  define NZ(s_singular, s_plural, count) wxU(ngettext(s_singular, s_plural, count))
# endif
#else /* HAVE_LIBINTL_H */
# if !defined Z
#  define Z(s) wxU(s)
#  define NZ(s_singular, s_plural, count) wxU(s_singular)
# endif
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

// PNG or icon loading

#if defined(SYS_WINDOWS)

# define wx_get_png_or_icon(X) wxIcon(wxT(#X))

#else  // defined(SYS_WINDOWS)

# define wx_get_png_or_icon(X) wx_get_icon_from_memory_impl(X ## _png_bin, sizeof(X ## _png_bin))

inline wxIcon
wx_get_icon_from_memory_impl(unsigned char const *data,
                             unsigned int length) {
  wxMemoryInputStream is(data, length);
  wxIcon icon;
  icon.CopyFromBitmap(wxBitmap(wxImage(is, wxBITMAP_TYPE_ANY, -1), -1));
  return icon;
}

#endif  // defined(SYS_WINDOWS)

#endif /* __WXCOMMON_H */
