/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the error exception class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __ERROR_H
#define __ERROR_H

#include <string>

using namespace std;

class MTX_DLL_API error_c {
private:
  string error;
public:
  error_c():
    error("unknown error") {
  }

  error_c(const char *_error):
    error(_error) {
  }

  error_c(const string &_error):
    error(_error) {
  }

  const char *get_error() const {
    return error.c_str();
  }

  operator const char *() const {
    return error.c_str();
  }
};

#endif // __ERROR_H
