/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  error.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definitions for the error exception class
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __ERROR_H
#define __ERROR_H

#include <string>

#include "common.h"

using namespace std;

class error_c {
private:
  string error;
public:
  error_c() {
    error = "unknown error";
  };
  error_c(char *nerror, bool freeit = false) {
    error = nerror;
    if (freeit)
      safefree(nerror);
  };
  error_c(const string &nerror) { error = nerror; };
  error_c(const error_c &e) { error = e.error; };
  const char *get_error() const { return error.c_str(); };
  operator const char *() const { return error.c_str(); };
};

#endif // __ERROR_H
