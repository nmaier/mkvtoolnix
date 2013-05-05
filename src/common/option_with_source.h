/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Option with a priority/source

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_OPTION_WITH_SOURCE_H
#define MTX_COMMON_OPTION_WITH_SOURCE_H

#include "common/common_pch.h"

#include <boost/optional.hpp>

enum option_source_e {
    OPTION_SOURCE_NONE         =  0
  , OPTION_SOURCE_BITSTREAM    = 10
  , OPTION_SOURCE_CONTAINER    = 20
  , OPTION_SOURCE_COMMAND_LINE = 30
};

template<typename T>
class option_with_source_c {
public:
protected:
  option_source_e m_source;
  boost::optional<T> m_value;

public:
  option_with_source_c()
    : m_source{OPTION_SOURCE_NONE}
  {
  }

  option_with_source_c(T const &value,
                       option_source_e source)
    : m_source{OPTION_SOURCE_NONE}
  {
    set(value, source);
  }

  operator bool()
    const {
    return !!m_value;
  }

  T const &
  get()
    const {
    if (!*this)
      throw std::logic_error{"not set yet"};
    return m_value.get();
  }

  option_source_e
  get_source()
    const {
    if (!*this)
      throw std::logic_error{"not set yet"};
    return m_source;
  }

  void
  set(T const &value,
      option_source_e source) {
    if (source < m_source)
      return;

    m_value  = value;
    m_source = source;
  }
};

#endif // MTX_COMMON_OPTION_WITH_SOURCE_H
