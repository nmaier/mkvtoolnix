/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Endian helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include "common/endian.h"

uint16_t
get_uint16_le(const void *buf) {
  return get_uint_le(buf, 2);
}

uint32_t
get_uint24_le(const void *buf) {
  return get_uint_le(buf, 3);
}

uint32_t
get_uint32_le(const void *buf) {
  return get_uint_le(buf, 4);
}

uint64_t
get_uint64_le(const void *buf) {
  return get_uint_le(buf, 8);
}

uint64_t
get_uint_le(const void *buf,
            int num_bytes) {
  int i;
  num_bytes          = std::min(std::max(1, num_bytes), 8);
  unsigned char *tmp = (unsigned char *) buf;
  uint64_t ret       = 0;
  for (i = num_bytes - 1; 0 <= i; --i)
    ret = (ret << 8) + (tmp[i] & 0xff);

  return ret;
}

uint16_t
get_uint16_be(const void *buf) {
  return get_uint_be(buf, 2);
}

uint32_t
get_uint24_be(const void *buf) {
  return get_uint_be(buf, 3);
}

uint32_t
get_uint32_be(const void *buf) {
  return get_uint_be(buf, 4);
}

uint64_t
get_uint64_be(const void *buf) {
  return get_uint_be(buf, 8);
}

uint64_t
get_uint_be(const void *buf,
            int num_bytes) {
  int i;
  num_bytes          = std::min(std::max(1, num_bytes), 8);
  unsigned char *tmp = (unsigned char *) buf;
  uint64_t ret       = 0;
  for (i = 0; num_bytes > i; ++i)
    ret = (ret << 8) + (tmp[i] & 0xff);

  return ret;
}

void
put_uint16_le(void *buf,
              uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
}

void
put_uint24_le(void *buf,
              uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
}

void
put_uint32_le(void *buf,
              uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
}

void
put_uint64_le(void *buf,
              uint64_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
  tmp[4] = (value >>= 8) & 0xff;
  tmp[5] = (value >>= 8) & 0xff;
  tmp[6] = (value >>= 8) & 0xff;
  tmp[7] = (value >>= 8) & 0xff;
}

void
put_uint16_be(void *buf,
              uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[1] = value & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

void
put_uint24_be(void *buf,
              uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[2] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

void
put_uint32_be(void *buf,
              uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[3] = value & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

void
put_uint64_be(void *buf,
              uint64_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[7] = value & 0xff;
  tmp[6] = (value >>= 8) & 0xff;
  tmp[5] = (value >>= 8) & 0xff;
  tmp[4] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}
