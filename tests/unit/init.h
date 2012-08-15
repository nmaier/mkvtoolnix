/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for helper functions for unit tests

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_TESTS_UNIT_INIT_H
#define MTX_TESTS_UNIT_INIT_H

#include "common/common_pch.h"

#include "gtest/gtest.h"

extern bool g_warning_issued;

namespace mtxut {

class mxerror_x: public std::exception {
protected:
  std::string m_message;

public:
  mxerror_x(std::string const &message)
    : m_message{message}
  {
  }

  virtual ~mxerror_x() throw() { }


  virtual const char *what() const throw() {
    return m_message.c_str();
  }

  virtual std::string error() const throw() {
    return m_message;
  }
};

void init_suite();
void init_case();

}

#endif // MTX_TESTS_UNIT_INIT_H
