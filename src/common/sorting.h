/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   sorting functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_SORTING_H
#define MTX_COMMON_SORTING_H

#include "common/common_pch.h"

#include <boost/lexical_cast.hpp>

#include "common/strings/utf8.h"

namespace mtx { namespace sort {

template<typename StrT>
class natural_element_c {
private:
  StrT m_content;
  uint64_t m_number;
  bool m_is_numeric;

public:
  natural_element_c(StrT const &content)
    : m_content{content}
    , m_number{0}
    , m_is_numeric{false}
  {
    try {
      m_number     = boost::lexical_cast<uint64_t>(m_content);
      m_is_numeric = true;
    } catch (boost::bad_lexical_cast &){
    }
  }

  bool
  operator <(natural_element_c const &b)
    const {
    if ( m_is_numeric &&  b.m_is_numeric)
      return m_number < b.m_number;
    if (!m_is_numeric && !b.m_is_numeric)
      return m_content < b.m_content;
    return m_is_numeric;
  }

  bool
  operator ==(natural_element_c const &b)
    const {
    return m_content == b.m_content;
  }
};

template<typename StrT>
class natural_string_c {
private:
  StrT m_original;
  std::vector<natural_element_c<std::wstring> > m_parts;

public:
  natural_string_c(StrT const &original)
    : m_original{original}
  {
    std::wstring wide = to_wide(m_original);
    boost::wregex re(L"\\d+", boost::regex::icase | boost::regex::perl);
    boost::wsregex_token_iterator it(wide.begin(), wide.end(), re, std::vector<int>{ -1, 0 });
    boost::wsregex_token_iterator end;
    while (it != end)
      m_parts.push_back(natural_element_c<std::wstring>{*it++});
  }

  StrT const &
  get_original()
    const {
    return m_original;
  }

  bool
  operator <(natural_string_c<StrT> const &b)
    const {
    auto min = std::min(m_parts.size(), b.m_parts.size());
    for (size_t idx = 0; idx < min; ++idx)
      if (m_parts[idx] < b.m_parts[idx])
        return true;
      else if (!(m_parts[idx] == b.m_parts[idx]))
        return false;
    return m_parts.size() < b.m_parts.size();
  }
};

template<typename StrT>
void
naturally(std::vector<StrT> &strings) {
  typedef std::pair<natural_string_c<StrT>, size_t> sorter_t;

  std::vector<sorter_t> to_sort;
  for (auto &string : strings)
    to_sort.push_back(std::make_pair(natural_string_c<StrT>{string}, to_sort.size()));

  brng::sort(to_sort, [&](sorter_t const &a, sorter_t const &b) { return a.first < b.first; });

  brng::transform(to_sort, strings.begin(), [](sorter_t const &e) { return e.first.get_original(); });
}

}}

#endif  // MTX_COMMON_SORTING_H
