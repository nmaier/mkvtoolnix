/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   CorePanorama types

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __MTX_COMMON_COREPICTURE_H
#define __MTX_COMMON_COREPICTURE_H

#include "common/common_pch.h"

enum corepicture_pic_type_e {
  COREPICTURE_TYPE_JPEG = 0,
  COREPICTURE_TYPE_PNG  = 1,
  COREPICTURE_TYPE_UNKNOWN
};

enum corepicture_panorama_type_e {
  COREPICTURE_PAN_FLAT       = 0,
  COREPICTURE_PAN_BASIC      = 1,
  COREPICTURE_PAN_WRAPAROUND = 2,
  COREPICTURE_PAN_SPHERICAL  = 3,
  COREPICTURE_PAN_UNKNOWN
};

#define COREPICTURE_USE_JPEG (1 << 0)
#define COREPICTURE_USE_PNG  (1 << 1)

struct corepicture_pic_t {
  int64_t m_time, m_end_time;
  std::string m_url;
  corepicture_pic_type_e m_pic_type;
  corepicture_panorama_type_e m_pan_type;

  corepicture_pic_t()
    : m_time(-1)
    , m_end_time(-1)
    , m_pic_type(COREPICTURE_TYPE_UNKNOWN)
    , m_pan_type(COREPICTURE_PAN_UNKNOWN)
  {
  }

  bool is_valid() const {
    return (COREPICTURE_TYPE_UNKNOWN != m_pic_type) && (COREPICTURE_PAN_UNKNOWN != m_pan_type) && (m_url != "") && (0 <= m_time);
  }

  bool operator <(const corepicture_pic_t &cmp) const {
    return m_time < cmp.m_time;
  }
};

#endif  // __MTX_COMMON_COREPICTURE_H
