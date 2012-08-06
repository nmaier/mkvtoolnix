/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the basic_timecode_c template class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_TIMECODE_H
#define MTX_COMMON_TIMECODE_H

#include "common/common_pch.h"

#include <boost/operators.hpp>

template<typename T>
class basic_timecode_c
  : boost::totally_ordered< basic_timecode_c<T>
  , boost::arithmetic< basic_timecode_c<T>
  > >
{
private:
  T m_timecode;
  bool m_valid;

  explicit basic_timecode_c<T>(T timecode)
    : m_timecode{timecode}
    , m_valid{true}
  {
  }

  basic_timecode_c<T>(T timecode, bool valid)
    : m_timecode{timecode}
    , m_valid{valid}
  {
  }

public:
  basic_timecode_c<T>()
    : m_timecode{}
    , m_valid{}
  {
  }

  // validity
  bool valid() const {
    return m_valid;
  }

  void reset() {
    m_valid    = false;
    m_timecode = 0;
  }

  // deconstruction
  T to_ns() const {
    return m_timecode;
  }

  T to_ns(T value_if_invalid) const {
    return m_valid ? m_timecode : value_if_invalid;
  }

  T to_us() const {
    return m_timecode / 1000;
  }

  T to_ms() const {
    return m_timecode / 1000000;
  }

  T to_s() const {
    return m_timecode / 1000000000;
  }

  T to_mpeg() const {
    return m_timecode * 9 / 100000;
  }

  // arithmetic
  basic_timecode_c<T> operator +=(basic_timecode_c<T> const &other) {
    if (!m_valid || !other.m_valid)
      reset();
    else
      m_timecode += other.m_timecode;
    return *this;
  }

  basic_timecode_c<T> operator -=(basic_timecode_c<T> const &other) {
    if (!m_valid || !other.m_valid)
      reset();
    else
      m_timecode -= other.m_timecode;
    return *this;
  }

  basic_timecode_c<T> operator *=(basic_timecode_c<T> const &other) {
    if (!m_valid || !other.m_valid)
      reset();
    else
      m_timecode *= other.m_timecode;
    return *this;
  }

  basic_timecode_c<T> operator /=(basic_timecode_c<T> const &other) {
    if (!m_valid || !other.m_valid)
      reset();
    else
      m_timecode /= other.m_timecode;
    return *this;
  }

  basic_timecode_c<T> abs() const {
    return basic_timecode_c<T>{std::abs(m_timecode), m_valid};
  }

  // comparison
  bool operator <(basic_timecode_c<T> const &other) const {
    return !m_valid &&  other.m_valid ? true
         :  m_valid && !other.m_valid ? false
         :  m_valid &&  other.m_valid ? m_timecode < other.m_timecode
         :                              false;
  }

  bool operator ==(basic_timecode_c<T> const &other) const {
    return (!m_valid && !other.m_valid) || (m_valid && other.m_valid && (m_timecode == other.m_timecode));
  }

  // construction
  static basic_timecode_c<T> ns(T value) {
    return basic_timecode_c<T>{value};
  }

  static basic_timecode_c<T> us(T value) {
    return basic_timecode_c<T>{value * 1000};
  }

  static basic_timecode_c<T> ms(T value) {
    return basic_timecode_c<T>{value * 1000000};
  }

  static basic_timecode_c<T> s(T value) {
    return basic_timecode_c<T>{value * 1000000000};
  }

  static basic_timecode_c<T> min(T value) {
    return basic_timecode_c<T>{value * 60 * 1000000000};
  }

  static basic_timecode_c<T> h(T value) {
    return basic_timecode_c<T>{value * 3600 * 1000000000};
  }

  static basic_timecode_c<T> mpeg(T value) {
    return basic_timecode_c<T>{value * 100000 / 9};
  }

  static basic_timecode_c<T> factor(T value) {
    return basic_timecode_c<T>{value};
  }
};

typedef basic_timecode_c<int64_t> timecode_c;

#endif  // MTX_COMMON_TIMECODE_H
