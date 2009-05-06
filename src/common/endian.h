/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Endian helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_ENDIAN_H
#define __MTX_COMMON_ENDIAN_H

#include "common/os.h"

#define get_fourcc(b) get_uint32_be(b)
uint16_t MTX_DLL_API get_uint16_le(const void *buf);
uint32_t MTX_DLL_API get_uint24_le(const void *buf);
uint32_t MTX_DLL_API get_uint32_le(const void *buf);
uint64_t MTX_DLL_API get_uint64_le(const void *buf);
uint64_t MTX_DLL_API get_uint_le(const void *buf, int max_bytes);
uint16_t MTX_DLL_API get_uint16_be(const void *buf);
uint32_t MTX_DLL_API get_uint24_be(const void *buf);
uint32_t MTX_DLL_API get_uint32_be(const void *buf);
uint64_t MTX_DLL_API get_uint64_be(const void *buf);
uint64_t MTX_DLL_API get_uint_be(const void *buf, int max_bytes);
void MTX_DLL_API put_uint16_le(void *buf, uint16_t value);
void MTX_DLL_API put_uint32_le(void *buf, uint32_t value);
void MTX_DLL_API put_uint64_le(void *buf, uint64_t value);
void MTX_DLL_API put_uint16_be(void *buf, uint16_t value);
void MTX_DLL_API put_uint32_be(void *buf, uint32_t value);
void MTX_DLL_API put_uint64_be(void *buf, uint64_t value);

#endif  // __MTX_COMMON_ENDIAN_H
