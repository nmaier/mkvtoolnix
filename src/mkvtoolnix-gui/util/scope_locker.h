#ifndef MTX_MKVTOOLNIX_GUI_SCOPE_LOCKER_H
#define MTX_MKVTOOLNIX_GUI_SCOPE_LOCKER_H

#include "common/common_pch.h"

#include <QMutex>

class ScopeLocker {
private:
  QMutex &m_mutex;

public:
  ScopeLocker(QMutex &mutex)
    : m_mutex{mutex}
  {
    m_mutex.lock();
  }

  ScopeLocker(ScopeLocker const &) = delete;
  ScopeLocker &operator =(ScopeLocker const &) = delete;

  virtual ~ScopeLocker() {
    m_mutex.unlock();
  }
};

#endif  // MTX_MKVTOOLNIX_GUI_SCOPE_LOCKER_H
