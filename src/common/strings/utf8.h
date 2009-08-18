/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_LOCALE_UTF8_H
#define __MTX_COMMON_LOCALE_UTF8_H

#include "common/os.h"

#include <string>

std::wstring MTX_DLL_API to_wide(const std::string &source);
std::string MTX_DLL_API to_utf8(const std::wstring &source);

#endif  // __MTX_COMMON_LOCALE_UTF8_H
