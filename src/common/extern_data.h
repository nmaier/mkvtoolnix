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

#include "os.h"

#include <string>

typedef struct {
  const char *name, *extensions;
} mime_type_t;

extern MTX_DLL_API const char *sub_charsets[];
extern MTX_DLL_API const mime_type_t mime_types[];
extern MTX_DLL_API const char *cctlds[];

std::string MTX_DLL_API guess_mime_type(std::string ext, bool is_file);
bool MTX_DLL_API is_valid_cctld(const std::string &s);

#endif // __MTX_COMMON_EXTERN_DATA_H
