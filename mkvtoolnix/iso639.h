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
  char *english_name, *iso639_2_code, *iso639_1_code;
} iso639_language_t;

extern iso639_language_t iso639_language_list[];

int is_valid_iso639_2_code(char *iso639_2_code);
char *get_iso639_english_name(char *iso639_2_code);
void list_iso639_languages();

#endif // __ISO639_H
