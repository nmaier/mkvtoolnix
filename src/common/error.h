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
protected:
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

  virtual ~error_c() {
  }

  virtual string get_error() const {
    return error;
  }
};

#endif // __ERROR_H
