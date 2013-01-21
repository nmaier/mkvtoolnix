/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definition for the split point class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_SPLIT_POINT_H
#define MTX_COMMON_SPLIT_POINT_H

#include "common/common_pch.h"

class split_point_t {
public:
  enum type_e {
    duration,
    size,
    timecode,
    chapter,
    parts,
    parts_frame_field,
    frame_field,
  };

  int64_t m_point;
  type_e m_type;
  bool m_use_once, m_discard, m_create_new_file;

public:
  split_point_t(int64_t point,
                type_e type,
                bool use_once,
                bool discard = false,
                bool create_new_file = true)
    : m_point{point}
    , m_type{type}
    , m_use_once{use_once}
    , m_discard{discard}
    , m_create_new_file{create_new_file}
  {
  }

  bool
  operator <(split_point_t const &rhs)
    const {
    return m_point < rhs.m_point;
  }

  std::string str() const;
};

#endif  // MTX_COMMON_SPLIT_POINT_H
