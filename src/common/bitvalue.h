/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   The "bitvalue_c" class definition

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_MTX_BITVALUE_H
#define __MTX_COMMON_MTX_BITVALUE_H

#include "common/common_pch.h"

#include "common/memory.h"

#include <ebml/EbmlBinary.h>

namespace mtx {
  class bitvalue_parser_x: public exception {
  protected:
    std::string m_message;
  public:
    bitvalue_parser_x(const std::string &message)  : m_message(message)       { }
    bitvalue_parser_x(const boost::format &message): m_message(message.str()) { }
    virtual ~bitvalue_parser_x() throw() { }

    virtual const char *what() const throw() {
      return m_message.c_str();
    }
  };
}

class bitvalue_c {
private:
  memory_cptr m_value;
public:
  bitvalue_c(int size);
  bitvalue_c(const bitvalue_c &src);
  bitvalue_c(std::string s, unsigned int allowed_bitlength = 0);
  bitvalue_c(const EbmlBinary &elt);
  virtual ~bitvalue_c();

  bitvalue_c &operator =(const bitvalue_c &src);
  bool operator ==(const bitvalue_c &cmp) const;
  unsigned char operator [](size_t index) const;

  inline bool empty() const {
    return 0 == m_value->get_size();
  }
  inline size_t size() const {
    return m_value->get_size() * 8;
  }
  void generate_random();
  unsigned char *data() const {
    return m_value->get_buffer();
  }
};
typedef counted_ptr<bitvalue_c> bitvalue_cptr;

#endif  // __MTX_COMMON_MTX_BITVALUE_H
