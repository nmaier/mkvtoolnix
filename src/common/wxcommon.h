/*
 * wxcommon.h
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * definitions for wxWindows
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __WXCOMMON_H
#define __WXCOMMON_H

#if defined(wxUSE_UNICODE) && wxUSE_UNICODE
//# error Sorry, mkvtoolnix cannot be compiled if wxWindows has been built with Unicode support.
# if defined(SYS_WINDOWS)
#  error Sorry, mkvtoolnix cannot be compiled on Windows if wxWindows has been built with Unicode support.
#  define wxU(s)                /* not implemented yet */
# else
#  define wxU(s) wxString(s, wxConvUTF8)
# endif
# define wxCS(s) ((const wchar_t *)(s).c_str())
# define wxMB(s) ((const char *)(s).mb_str(wxConvUTF8))
# define wxMBL(s) ((const char *)(s).mb_str(wxConvLocal))
# define wxUCS(s) wxU(s).c_str()
# define wxCS2WS(s) wxUCS((s).c_str())
# define WXUNICODE 1
#else
# define wxU(s) wxString(s)
# define wxCS(s) ((const char *)(s).c_str())
# define wxMB(s) ((const char *)(s).c_str())
# define wxMBL(s) ((const char *)(s).c_str())
# define wxCS2WS(s) ((const char *)(s).c_str())
# define wxUCS(s) wxString(s).c_str()
# define WXUNICODE 0
#endif

#endif /* __WXCOMMON_H */
