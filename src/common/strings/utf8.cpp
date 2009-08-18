/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.

   These functions should normally reside in locale.cpp. But the
   'utf8' namespace declared by utf8.h clases with the 'utf8' symbol
   in the 'libebml' namespace and most of the common headers contain
   'using namespace libebml'.
*/

#include "common/os.h"

#include <string>
#include <utf8.h>

#include "common/strings/utf8.h"

std::wstring
to_wide(const std::string &source) {
  std::wstring destination;

  utf8::utf8to32(source.begin(), source.end(), back_inserter(destination));

  return destination;
}

std::string
to_utf8(const std::wstring &source) {
  std::string destination;

  utf8::utf32to8(source.begin(), source.end(), back_inserter(destination));

  return destination;
}

