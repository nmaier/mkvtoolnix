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
    \version \$Id: error.h,v 1.6 2003/03/03 18:00:30 mosu Exp $
    \brief class definitions for the error exception class
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __ERROR_H
#define __ERROR_H

#include <malloc.h>
#include <string.h>

#include "common.h"

class error_c {
private:
  char *error;
public:
  error_c(char *nerror) { error = nerror; };
  char *get_error()     { return error; };
};

#endif // __ERROR_H
