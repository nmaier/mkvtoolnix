/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   A class for file-like access on the bit level

   The bit_cursor_c class was originally written by Peter Niemayer
     <niemayer@isg.de> and modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __BIT_CURSOR_H
#define __BIT_CURSOR_H

#include "os.h"

#include "error.h"

class MTX_DLL_API bit_cursor_c {
private:
  const unsigned char *end_of_data;
  const unsigned char *byte_position;
  const unsigned char *start_of_data;
  unsigned int bits_valid;

  bool out_of_data;

public:
  bit_cursor_c(const unsigned char *data, unsigned int len):
    end_of_data(data + len), byte_position(data), start_of_data(data),
    bits_valid(8), out_of_data(false) {
    if (byte_position >= end_of_data)
      out_of_data = true;
  }

  bool eof() {
    return out_of_data;
  }

  bool get_bits(unsigned int n, uint64_t &r) {
    // returns false if less bits are available than asked for
    r = 0;

    while (n > 0) {
      if (byte_position >= end_of_data) {
        out_of_data = true;
        return false;
      }

      unsigned int b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > bits_valid)
        b = bits_valid;

      unsigned int rshift = bits_valid-b;

      r <<= b;
      r |= ((*byte_position) >> rshift) & (0xff >> (8 - b));

      bits_valid -= b;
      if (bits_valid == 0) {
        bits_valid = 8;
        byte_position += 1;
      }

      n -= b;
    }

    return true;
  }

  bool get_bits(unsigned int n, int64_t &r) {
    uint64_t t;
    bool b = get_bits(n, t);
    r = (int64_t)t;
    return b;
  }

  bool get_bits(unsigned int n, int &r) {
    uint64_t t;
    bool b = get_bits(n, t);
    r = (int)t;
    return b;
  }

  bool get_bits(unsigned int n, unsigned int &r) {
    uint64_t t;
    bool b = get_bits(n, t);
    r = (unsigned int)t;
    return b;
  }

  bool get_bit(bool &r) {
    uint64_t t;
    bool b = get_bits(1, t);
    r = (bool)t;
    return b;
  }

  bool peek_bits(unsigned int n, uint64_t &r) {
    int tmp_bits_valid;
    const unsigned char *tmp_byte_position;
    // returns false if less bits are available than asked for
    r = 0;
    tmp_byte_position = byte_position;
    tmp_bits_valid = bits_valid;

    while (n > 0) {
      if (tmp_byte_position >= end_of_data)
        return false;

      unsigned int b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > tmp_bits_valid)
        b = tmp_bits_valid;

      unsigned int rshift = tmp_bits_valid - b;

      r <<= b;
      r |= ((*tmp_byte_position) >> rshift) & (0xff >> (8 - b));

      tmp_bits_valid -= b;
      if (tmp_bits_valid == 0) {
        tmp_bits_valid = 8;
        tmp_byte_position += 1;
      }

      n -= b;
    }

    return true;
  }

  bool peek_bits(unsigned int n, int64_t &r) {
    uint64_t t;
    bool b = peek_bits(n, t);
    r = (int64_t)t;
    return b;
  }

  bool peek_bits(unsigned int n, int &r) {
    uint64_t t;
    bool b = peek_bits(n, t);
    r = (int)t;
    return b;
  }

  bool peek_bits(unsigned int n, unsigned int &r) {
    uint64_t t;
    bool b = peek_bits(n, t);
    r = (unsigned int)t;
    return b;
  }

  bool peek_bit(bool &r) {
    uint64_t t;
    bool b = peek_bits(1, t);
    r = (bool)t;
    return b;
  }

  bool byte_align() {
    if (out_of_data)
      return false;
    if (bits_valid == 8)
      return true;
    bits_valid = 0;
    byte_position += 1;
    return true;
  }

  bool set_bit_position(unsigned int pos) {
    if (pos >= ((end_of_data - start_of_data) * 8)) {
      byte_position = end_of_data;
      out_of_data = true;
      return false;
    }

    byte_position = start_of_data + (pos / 8);
    bits_valid = 8 - (pos % 8);

    return true;
  }

  int get_bit_position() {
    return (byte_position - start_of_data) * 8 + 8 - bits_valid;
  }

  bool skip_bits(unsigned int num) {
    return set_bit_position(get_bit_position() + num);
  }
};

#endif // __BIT_CURSOR_H
