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
#include <stdexcept>

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
    m_timecode = std::numeric_limits<T>::min();
  }

  // deconstruction
  T to_ns() const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    return m_timecode;
  }

  T to_ns(T value_if_invalid) const {
    return m_valid ? m_timecode : value_if_invalid;
  }

  T to_us() const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    return m_timecode / 1000;
  }

  T to_ms() const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    return m_timecode / 1000000;
  }

  T to_s() const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    return m_timecode / 1000000000;
  }

  T to_m() const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    return m_timecode / 60000000000ll;
  }

  T to_h() const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    return m_timecode / 3600000000000ll;
  }

  T to_mpeg() const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    return m_timecode * 9 / 100000;
  }

  T to_samples(T sample_rate) const {
    if (!m_valid)
      throw std::domain_error{"invalid timecode"};
    if (!sample_rate)
      throw std::domain_error{"invalid sample rate"};
    return (m_timecode * sample_rate / 100000000 + 5) / 10;
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

  static basic_timecode_c<T> m(T value) {
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

  static basic_timecode_c<T> samples(T samples, T sample_rate) {
    if (!sample_rate)
      throw std::domain_error{"invalid sample rate"};
    return basic_timecode_c<T>{(samples * 10000000000 / sample_rate + 5) / 10};
  }
};

typedef basic_timecode_c<int64_t> timecode_c;

#endif  // MTX_COMMON_TIMECODE_H
