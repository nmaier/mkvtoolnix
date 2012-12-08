/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for a "variable sized integer" helper class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_VINT_H
#define MTX_COMMON_VINT_H

#include "common/common_pch.h"

#include <ebml/EbmlId.h>

#include "common/mm_io.h"

class vint_c {
public:
  int64_t m_value;
  int m_coded_size;
  bool m_is_set;

public:
  enum read_mode_e {
    rm_normal,
    rm_ebml_id,
  };

  vint_c();
  vint_c(int64_t value, int coded_size);
  bool is_unknown();
  bool is_valid();

  operator EbmlId() const;

public:                         // static functions
  static vint_c read(mm_io_c *in, read_mode_e read_mode = rm_normal);
  static vint_c read(mm_io_cptr &in, read_mode_e read_mode = rm_normal);

  static vint_c read_ebml_id(mm_io_c *in);
  static vint_c read_ebml_id(mm_io_cptr &in);
};

#endif  // MTX_COMMON_VINT_H
