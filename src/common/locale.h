/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_LOCALE_H
#define __MTX_COMMON_LOCALE_H

#include "common/os.h"

#include <string>
#include <vector>

extern int MTX_DLL_API cc_local_utf8;

std::string MTX_DLL_API get_local_charset();
std::string MTX_DLL_API get_local_console_charset();

int MTX_DLL_API utf8_init(const std::string &charset);
void MTX_DLL_API utf8_done();

std::string MTX_DLL_API to_utf8(int handle, const std::string &local);
std::string MTX_DLL_API from_utf8(int handle, const std::string &utf8);

std::vector<std::string> MTX_DLL_API command_line_utf8(int argc, char **argv);

# ifdef SYS_WINDOWS
unsigned MTX_DLL_API Utf8ToUtf16(const char *utf8, int utf8len, wchar_t *utf16, unsigned utf16len);
char * MTX_DLL_API win32_wide_to_multi(const wchar_t *wbuffer);
wchar_t * MTX_DLL_API win32_utf8_to_utf16(const char *s);
std::string MTX_DLL_API win32_wide_to_multi_utf8(const wchar_t *wbuffer);
# endif

#endif  // __MTX_COMMON_LOCALE_H
