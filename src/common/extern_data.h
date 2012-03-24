/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for ComboBox data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_EXTERN_DATA_H
#define __MTX_COMMON_EXTERN_DATA_H

#include "common/common_pch.h"

struct mime_type_t {
  std::string const name;
  std::vector<std::string> const extensions;
};

extern std::vector<std::string> const sub_charsets, cctlds;
extern std::vector<mime_type_t> const mime_types;

std::string guess_mime_type(std::string ext, bool is_file);
bool is_valid_cctld(const std::string &s);

#endif // __MTX_COMMON_EXTERN_DATA_H
