/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   objects for managing translations

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "mmg/translation_table.h"

translation_table_c::translation_table_c()
{
}

void
translation_table_c::add(const wxString &english,
                         const wxString &translated) {
  entries.push_back(translation_t(english, translated));
}

void
translation_table_c::clear() {
  entries.clear();
}

wxString
translation_table_c::to_translated(const wxString &english) {
  std::vector<translation_t>::iterator i = entries.begin();

  while (i != entries.end()) {
    if (i->english == english)
      return i->translated;
    ++i;
  }

  return english;
}

wxString
translation_table_c::to_english(const wxString &translated) {
  std::vector<translation_t>::iterator i = entries.begin();

  while (i != entries.end()) {
    if (i->translated == translated)
      return i->english;
    ++i;
  }

  return translated;
}
