/*
   mkvpropedit -- utility for editing properties of existing Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_PROPEDIT_TARGET_ID_MANAGER_H
#define MTX_PROPEDIT_TARGET_ID_MANAGER_H

#include "common/common_pch.h"

#include <ebml/EbmlMaster.h>

template<typename T>
class target_id_manager_c {
private:
  std::vector<T *> m_targets;
  int m_offset;

public:
  target_id_manager_c(::libebml::EbmlMaster *container, int offset = 0);

  T *get(int id) const;
  bool has(int id) const;
  void remove(int id);

private:
  bool id_ok(int id) const;
};

template<typename T>
target_id_manager_c<T>::target_id_manager_c(::libebml::EbmlMaster *container,
                                            int offset)
  : m_offset{offset}
{
  auto list_size = container->ListSize();

  m_targets.reserve(list_size);

  for (auto element : *container)
    if (dynamic_cast<T *>(element))
      m_targets.push_back(static_cast<T *>(element));
}

template<typename T>
T*
target_id_manager_c<T>::get(int id)
  const {
  return id_ok(id) ? m_targets[id - m_offset] : nullptr;
}

template<typename T>
bool
target_id_manager_c<T>::has(int id)
  const {
  return id_ok(id) ? !!m_targets[id - m_offset] : false;
}

template<typename T>
bool
target_id_manager_c<T>::id_ok(int id)
  const {
  auto idx = id - m_offset;
  return (0 <= idx) && (static_cast<int>(m_targets.size()) > idx);
}

template<typename T>
void
target_id_manager_c<T>::remove(int id) {
  if (id_ok(id))
    m_targets[id - m_offset] = nullptr;
}

#endif  // MTX_PROPEDIT_TARGET_ID_MANAGER_H
