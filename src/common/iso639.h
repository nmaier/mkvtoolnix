/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   ISO639 language definitions, lookup functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_ISO639_H
#define __MTX_COMMON_ISO639_H

#include "common/os.h"

typedef struct {
  const char *english_name, *iso639_2_code, *iso639_1_code;
  const char *terminology_abbrev;
} iso639_language_t;

extern const iso639_language_t MTX_DLL_API iso639_languages[];

int MTX_DLL_API map_to_iso639_2_code(const char *s, bool allow_short_english_names = false);
bool MTX_DLL_API is_valid_iso639_2_code(const char *iso639_2_code);
const char *MTX_DLL_API map_iso639_2_to_iso639_1(const char *iso639_2_code);
void MTX_DLL_API list_iso639_languages();
bool MTX_DLL_API is_popular_language(const char *lang);
bool MTX_DLL_API is_popular_language_code(const char *code);

#endif // __MTX_COMMON_ISO639_H
