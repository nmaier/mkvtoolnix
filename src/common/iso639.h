/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  iso639.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief ISO639 language definitions, lookup functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __ISO639_H
#define __ISO639_H

typedef struct {
  const char *english_name, *iso639_2_code, *iso639_1_code;
} iso639_language_t;

extern const iso639_language_t MTX_DLL_API iso639_languages[];

int MTX_DLL_API is_valid_iso639_2_code(const char *iso639_2_code);
const char *MTX_DLL_API get_iso639_english_name(const char *iso639_2_code);
const char *MTX_DLL_API map_iso639_1_to_iso639_2(const char *iso639_1_code);
const char *MTX_DLL_API map_iso639_2_to_iso639_1(const char *iso639_2_code);
void MTX_DLL_API list_iso639_languages();

#endif // __ISO639_H
