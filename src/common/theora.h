/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Ogg Theora helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_THEORA_COMMON_H
#define MTX_COMMON_THEORA_COMMON_H

#include "common/common_pch.h"

#include "common/error.h"

#define THEORA_HEADERTYPE_IDENTIFICATION 0x80
#define THEORA_HEADERTYPE_COMMENT        0x81
#define THEORA_HEADERTYPE_SETUP          0x82

namespace mtx {
  namespace theora {
    class header_parsing_x: public mtx::exception {
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

struct theora_identification_header_t {
  uint8_t headertype;
  char theora_string[6];

  uint8_t vmaj;
  uint8_t vmin;
  uint8_t vrev;

  uint16_t fmbw;
  uint16_t fmbh;

  uint32_t picw;
  uint32_t pich;
  uint8_t picx;
  uint8_t picy;

  uint32_t frn;
  uint32_t frd;

  uint32_t parn;
  uint32_t pard;

  uint8_t cs;
  uint8_t pf;

  uint32_t nombr;
  uint8_t qual;

  uint8_t kfgshift;

  int display_width, display_height;

  theora_identification_header_t();
};

void theora_parse_identification_header(unsigned char *buffer, int size, theora_identification_header_t &header);

#endif // MTX_COMMON_THEORA_COMMON_H
