#ifndef MTX_MMGQT_MMG_QT_H
#define MTX_MMGQT_MMG_QT_H

#include "common/common_pch.h"

#include <QString>

#include "common/qt.h"

struct mkvmerge_settings_t {
  enum process_priority_e {
    priority_lowest,
    priority_low,
    priority_normal,
    priority_high,
    priority_highest,
  };

  QString executable;
  process_priority_e priority;

  mkvmerge_settings_t();
};

#endif  // MTX_MMGQT_MMG_QT_H
