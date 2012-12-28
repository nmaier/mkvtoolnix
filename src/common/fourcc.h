/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for FourCC handling

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_FOURCC_H
#define MTX_COMMON_FOURCC_H

#include "common/common_pch.h"

#include <ostream>

#include "common/mm_io.h"

class fourcc_c {
public:
  typedef enum { big_endian, little_endian } byte_order_t;

private:
  uint32_t m_value;

public:
  fourcc_c();

  // From an integer value:
  fourcc_c(uint32_t value, byte_order_t byte_order = big_endian);

  // From strings:
  fourcc_c(std::string const &value);
  fourcc_c(char const *value);

  // From memory:
  fourcc_c(memory_cptr const &mem, byte_order_t byte_order = big_endian);
  fourcc_c(unsigned char const *mem, byte_order_t byte_order = big_endian);
  fourcc_c(uint32_t const *mem, byte_order_t byte_order = big_endian);

  // From mm_io_c instances:
  fourcc_c(mm_io_cptr const &io, byte_order_t byte_order = big_endian);
  fourcc_c(mm_io_c &io, byte_order_t byte_order = big_endian);
  fourcc_c(mm_io_c *io, byte_order_t byte_order = big_endian);

  size_t write(memory_cptr const &mem, byte_order_t byte_order = big_endian);
  size_t write(unsigned char *mem, byte_order_t byte_order = big_endian);
  size_t write(mm_io_cptr const &io, byte_order_t byte_order = big_endian);
  size_t write(mm_io_c &io, byte_order_t byte_order = big_endian);
  size_t write(mm_io_c *io, byte_order_t byte_order = big_endian);

  fourcc_c &reset();

  uint32_t value(byte_order_t byte_order = big_endian) const;
  std::string str() const;

  bool equiv(char const *cmp) const;

  explicit operator bool() const;
  bool operator ==(fourcc_c const &cmp) const;
  bool operator !=(fourcc_c const &cmp) const;

protected:
  // From memory & strings:
  static uint32_t read(void const *mem, byte_order_t byte_order);
  // From mm_io_c instances:
  static uint32_t read(mm_io_c &io, byte_order_t byte_order);
};

inline std::ostream &
operator <<(std::ostream &out,
            fourcc_c const &fourcc) {
  out << fourcc.str();
  return out;
}

#endif  // MTX_COMMON_FOURCC_H
