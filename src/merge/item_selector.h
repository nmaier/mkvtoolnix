/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for item selector class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MERGE_ITEM_SELECTOR_H
#define MTX_MERGE_ITEM_SELECTOR_H

#include "common/common_pch.h"

template<typename T>
class item_selector_c {
public:
  T m_default_value;
  std::map<int64_t, T> m_items;
  bool m_none, m_reversed;

public:
  item_selector_c(T default_value = T(0))
    : m_default_value(default_value)
    , m_none(false)
    , m_reversed(false)
  {
  }

  bool selected(int64_t item) {
    if (m_none)
      return false;

    if (m_items.empty())
      return !m_reversed;

    bool included = m_items.end() != m_items.find(item);
    return m_reversed ? !included : included;
  }

  T get(int64_t item) {
    return selected(item) ? m_items[item] : m_default_value;
  }

  bool none() {
    return m_none;
  }

  void set_none() {
    m_none = true;
  }

  void set_reversed() {
    m_reversed = true;
  }

  void add(int64_t item, T value = T(0)) {
    m_items[item] = value;
  }

  void clear() {
    m_items.clear();
  }

  bool empty() {
    return m_items.empty();
  }
};

#endif  // MTX_MERGE_ITEM_SELECTOR_H
