/*
   mkvtoolnix - programs for manipulating Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   base64 encoding and decoding functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_BASE64_H
#define __MTX_COMMON_BASE64_H

#include "common/common_pch.h"

namespace mtx {
  namespace base64 {
    class exception: public mtx::exception {
    public:
      virtual const char *what() const throw() {
        return "unspecified Base64 encoder/decoder error";
      }
    };

    class invalid_data_x: public exception {
    public:
      virtual const char *what() const throw() {
        return Y("Invalid Base64 character encountered");
      }
    };
  }
}

std::string base64_encode(const unsigned char *src, int src_len, bool line_breaks = false, int max_line_len = 72);
int base64_decode(const std::string &src, unsigned char *dst);

#endif // __MTX_COMMON_BASE64_H
