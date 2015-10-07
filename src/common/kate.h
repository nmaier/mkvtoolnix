/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Kate helper functions

   Written by ogg.k.ogg.k <ogg.k.ogg.k@googlemail.com>.
   Adapted from code by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_KATE_COMMON_H
#define MTX_COMMON_KATE_COMMON_H

#include "common/common_pch.h"

#include "common/error.h"

#define KATE_HEADERTYPE_IDENTIFICATION 0x80

namespace mtx {
  namespace kate {
    class header_parsing_x: public exception {
    protected:
      std::string m_message;
    public:
      header_parsing_x(const std::string &message)  : m_message(message)       { }
      header_parsing_x(const boost::format &message): m_message(message.str()) { }
      virtual ~header_parsing_x() throw() { }

      virtual const char *what() const throw() {
        return m_message.c_str();
      }
    };
  }
}

struct kate_identification_header_t {
  uint8_t headertype;
  char kate_string[7];

  uint8_t vmaj;
  uint8_t vmin;

  uint8_t nheaders;
  uint8_t tenc;
  uint8_t tdir;

  uint8_t kfgshift;

  int32_t gnum;
  int32_t gden;

  uint8_t language[16];
  uint8_t category[16];

  kate_identification_header_t();
};

void kate_parse_identification_header(const unsigned char *buffer, int size, kate_identification_header_t &header);

#endif // MTX_COMMON_KATE_COMMON_H
