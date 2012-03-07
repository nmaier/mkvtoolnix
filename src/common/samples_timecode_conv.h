/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the samples-to-timecode converter

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_SAMPLES_TIMECODE_CONV_H
#define __MTX_COMMON_SAMPLES_TIMECODE_CONV_H

#include "common/common_pch.h"

class samples_to_timecode_converter_c {
protected:
  int64_t m_numerator, m_denominator;

public:
  samples_to_timecode_converter_c()
    : m_numerator(0)
    , m_denominator(0)
  { }

  samples_to_timecode_converter_c(int64_t numerator, int64_t denominator)
    : m_numerator(0)
    , m_denominator(0)
  {
    set(numerator, denominator);
  }

  void set(int64_t numerator, int64_t denominator) {
    if (0 == denominator)
      return;

    int64_t gcd   = boost::math::gcd(numerator, denominator);

    m_numerator   = numerator   / gcd;
    m_denominator = denominator / gcd;
  }

  inline int64_t operator *(int64_t v1) {
    return v1 * *this;
  }

  friend int64_t operator *(int64_t v1, const samples_to_timecode_converter_c &v2);
};

inline int64_t
operator *(int64_t v1,
           const samples_to_timecode_converter_c &v2) {
  return v2.m_denominator ? v1 * v2.m_numerator / v2.m_denominator : v1;
}

#endif  // __MTX_COMMON_SAMPLES_TIMECODE_CONV_H
