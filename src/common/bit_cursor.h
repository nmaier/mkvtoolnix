/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   A class for file-like access on the bit level

   The bit_cursor_c class was originally written by Peter Niemayer
     <niemayer@isg.de> and modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_BIT_CURSOR_H
#define MTX_COMMON_BIT_CURSOR_H

#include "common/common_pch.h"

#include "common/mm_io_x.h"

class bit_cursor_c {
private:
  const unsigned char *m_end_of_data;
  const unsigned char *m_byte_position;
  const unsigned char *m_start_of_data;
  unsigned int m_bits_valid;
  bool m_out_of_data;

public:
  bit_cursor_c(const unsigned char *data, unsigned int len) {
    init(data, len);
  }

  void init(const unsigned char *data, unsigned int len) {
    m_end_of_data   = data + len;
    m_byte_position = data;
    m_start_of_data = data;
    m_bits_valid    = 8;
    m_out_of_data   = m_byte_position >= m_end_of_data;
  }

  bool eof() {
    return m_out_of_data;
  }

  uint64_t get_bits(unsigned int n) {
    uint64_t r = 0;

    while (n > 0) {
      if (m_byte_position >= m_end_of_data) {
        m_out_of_data = true;
        throw mtx::mm_io::end_of_file_x();
      }

      unsigned int b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > m_bits_valid)
        b = m_bits_valid;

      unsigned int rshift = m_bits_valid - b;

      r <<= b;
      r  |= ((*m_byte_position) >> rshift) & (0xff >> (8 - b));

      m_bits_valid -= b;
      if (0 == m_bits_valid) {
        m_bits_valid     = 8;
        m_byte_position += 1;
      }

      n -= b;
    }

    return r;
  }

  inline int get_bit() {
    return get_bits(1);
  }

  inline int get_unary(bool stop,
                       int len) {
    int i;

    for (i = 0; (i < len) && get_bit() != stop; ++i)
      ;

    return i;
  }

  inline int get_012() {
    if (!get_bit())
      return 0;
    return get_bits(1) + 1;
  }

  uint64_t peek_bits(unsigned int n) {
    uint64_t r                             = 0;
    const unsigned char *tmp_byte_position = m_byte_position;
    unsigned int tmp_bits_valid            = m_bits_valid;

    while (0 < n) {
      if (tmp_byte_position >= m_end_of_data)
        throw mtx::mm_io::end_of_file_x();

      unsigned int b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > tmp_bits_valid)
        b = tmp_bits_valid;

      unsigned int rshift = tmp_bits_valid - b;

      r <<= b;
      r  |= ((*tmp_byte_position) >> rshift) & (0xff >> (8 - b));

      tmp_bits_valid -= b;
      if (0 == tmp_bits_valid) {
        tmp_bits_valid     = 8;
        tmp_byte_position += 1;
      }

      n -= b;
    }

    return r;
  }

  void get_bytes(unsigned char *buf, size_t n) {
    size_t idx;
    for (idx = 0; idx < n; ++idx)
      buf[idx] = get_bits(8);
  }

  void byte_align() {
    if (8 == m_bits_valid)
      return;

    if (m_out_of_data)
      throw mtx::mm_io::end_of_file_x();

    m_bits_valid     = 0;
    m_byte_position += 1;
  }

  void set_bit_position(unsigned int pos) {
    if (pos >= (static_cast<unsigned int>(m_end_of_data - m_start_of_data) * 8)) {
      m_byte_position = m_end_of_data;
      m_out_of_data   = true;

      throw mtx::mm_io::end_of_file_x();
    }

    m_byte_position = m_start_of_data + (pos / 8);
    m_bits_valid    = 8 - (pos % 8);
  }

  int get_bit_position() {
    return (m_byte_position - m_start_of_data) * 8 + 8 - m_bits_valid;
  }

  void skip_bits(unsigned int num) {
    set_bit_position(get_bit_position() + num);
  }
};

class bit_writer_c {
private:
  unsigned char *m_end_of_data;
  unsigned char *m_byte_position;
  unsigned char *m_start_of_data;
  unsigned int m_mask;

  bool m_out_of_data;

public:
  bit_writer_c(unsigned char *data, unsigned int len)
    : m_end_of_data(data + len)
    , m_byte_position(data)
    , m_start_of_data(data)
    , m_mask(0x80)
    , m_out_of_data(m_byte_position >= m_end_of_data)
  {
  }

  uint64_t copy_bits(unsigned int n, bit_cursor_c &src) {
    uint64_t value = src.get_bits(n);
    put_bits(n, value);

    return value;
  }

  void put_bits(unsigned int n, uint64_t value) {
    while (0 < n) {
      put_bit(value & (1 << (n - 1)));
      --n;
    }
  }

  void put_bit(bool bit) {
    if (m_byte_position >= m_end_of_data) {
      m_out_of_data = true;
      throw mtx::mm_io::end_of_file_x();
    }

    if (bit)
      *m_byte_position |=  m_mask;
    else
      *m_byte_position &= ~m_mask;
    m_mask >>= 1;
    if (0 == m_mask) {
      m_mask = 0x80;
      ++m_byte_position;
      if (m_byte_position == m_end_of_data)
        m_out_of_data = true;
    }
  }

  void byte_align() {
    while (0x80 != m_mask)
      put_bit(0);
  }

  void set_bit_position(unsigned int pos) {
    if (pos >= (static_cast<unsigned int>(m_end_of_data - m_start_of_data) * 8)) {
      m_byte_position = m_end_of_data;
      m_out_of_data   = true;

      throw mtx::mm_io::seek_x();
    }

    m_byte_position = m_start_of_data + (pos / 8);
    m_mask          = 0x80 >> (pos % 8);
  }

  int get_bit_position() {
    unsigned int i;
    unsigned int pos = (m_byte_position - m_start_of_data) * 8;
    for (i = 0; 8 > i; ++i)
      if ((0x80u >> i) == m_mask) {
        pos += i;
        break;
      }
    return pos;
  }
};
typedef std::shared_ptr<bit_cursor_c> bit_cursor_cptr;

#endif // MTX_COMMON_BIT_CURSOR_H
