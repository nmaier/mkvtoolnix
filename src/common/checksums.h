/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   checksum calculations

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __CHECKSUMS_H
#define __CHECKSUMS_H

#include "os.h"

uint32_t MTX_DLL_API calc_adler32(const unsigned char *buffer, int size);
uint32_t MTX_DLL_API calc_crc32(const unsigned char *buffer, int size);

#endif // __CHECKSUMS_H
