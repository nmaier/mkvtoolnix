/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Helper functions for creating unique numbers.

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_UNIQUE_NUMBERS_H
#define __MTX_COMMON_UNIQUE_NUMBERS_H

#include "common/os.h"

enum unique_id_category_e {
  UNIQUE_ALL_IDS        = -1,
  UNIQUE_TRACK_IDS      = 0,
  UNIQUE_CHAPTER_IDS    = 1,
  UNIQUE_EDITION_IDS    = 2,
  UNIQUE_ATTACHMENT_IDS = 3
};

void MTX_DLL_API clear_list_of_unique_uint32(unique_id_category_e category);
bool MTX_DLL_API is_unique_uint32(uint32_t number, unique_id_category_e category);
void MTX_DLL_API add_unique_uint32(uint32_t number, unique_id_category_e category);
void MTX_DLL_API remove_unique_uint32(uint32_t number, unique_id_category_e category);
uint32_t MTX_DLL_API create_unique_uint32(unique_id_category_e category);

#endif  // __MTX_COMMON_UNIQUE_NUMBERS_H
