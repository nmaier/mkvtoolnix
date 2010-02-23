/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for a "variable sized integer" helper class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_VINT_H
#define __MTX_COMMON_VINT_H

#include "common/os.h"

#include "common/mm_io.h"

class vint_c {
public:
  int64_t m_value;
  int m_coded_size;
  bool m_is_set;

public:
  vint_c();
  vint_c(int64_t value, int coded_size);
  bool is_unknown();
  bool is_valid();

public:                         // static functions
  static vint_c read(mm_io_c *in);
  static vint_c read(mm_io_cptr &in);
};

#endif  // __MTX_COMMON_VINT_H
