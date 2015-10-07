/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   a "variable sized integer" helper class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/


#include "common/common_pch.h"

#include "common/vint.h"

vint_c::vint_c()
  : m_value(0)
  , m_coded_size(-1)
  , m_is_set(false)
{
}

vint_c::vint_c(int64_t value,
               int coded_size)
  : m_value(value)
  , m_coded_size(coded_size)
  , m_is_set(true)
{
}

bool
vint_c::is_unknown() {
  return !is_valid()
    || ((1 == m_coded_size) && (0x000000000000007fll == m_value))
    || ((2 == m_coded_size) && (0x0000000000003fffll == m_value))
    || ((3 == m_coded_size) && (0x00000000001fffffll == m_value))
    || ((4 == m_coded_size) && (0x000000000fffffffll == m_value))
    || ((5 == m_coded_size) && (0x00000007ffffffffll == m_value))
    || ((6 == m_coded_size) && (0x000003ffffffffffll == m_value))
    || ((7 == m_coded_size) && (0x0001ffffffffffffll == m_value))
    || ((8 == m_coded_size) && (0x00ffffffffffffffll == m_value));
}

bool
vint_c::is_valid() {
  return m_is_set && (0 <= m_coded_size);
}

vint_c
vint_c::read(mm_io_c *in,
             vint_c::read_mode_e read_mode) {
  int64_t pos       = in->getFilePointer();
  int64_t file_size = in->get_size();
  int mask          = 0x80;
  int value_len      = 1;

  if (pos >= file_size)
    return vint_c();

  unsigned char first_byte = in->read_uint8();

  while (0 != mask) {
    if (0 != (first_byte & mask))
      break;

    mask >>= 1;
    value_len++;
  }

  if ((pos + value_len) > file_size)
    return vint_c();

  if (   (rm_ebml_id == read_mode)
      && (   (0 == mask)
          || (4 <  value_len)))
    return vint_c();

  int64_t value = first_byte;
  if (rm_normal == read_mode)
    value &= ~mask;

  int i;
  for (i = 1; i < value_len; ++i) {
    value <<= 8;
    value  |= in->read_uint8();
  }

  return vint_c(value, value_len);
}

vint_c
vint_c::read_ebml_id(mm_io_c *in) {
  return read(in, rm_ebml_id);
}

vint_c
vint_c::read(mm_io_cptr &in,
             vint_c::read_mode_e read_mode) {
  return read(in.get(), read_mode);
}

vint_c
vint_c::read_ebml_id(mm_io_cptr &in) {
  return read(in.get(), rm_ebml_id);
}

vint_c::operator EbmlId()
  const {
  return EbmlId(m_value, m_coded_size);
}

