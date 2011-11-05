/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the error exception class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_ERROR_H
#define __MTX_COMMON_ERROR_H

#include "common/common_pch.h"


class error_c {
protected:
  std::string error;

public:
  error_c():
    error("unknown error") {
  }

  error_c(const char *_error):
    error(_error) {
  }

  error_c(const std::string &_error):
    error(_error) {
  }

  error_c(const boost::format &format)
    : error(format.str()) {
  }

  virtual ~error_c() {
  }

  virtual std::string get_error() const {
    return error;
  }
};

#endif // __MTX_COMMON_ERROR_H
