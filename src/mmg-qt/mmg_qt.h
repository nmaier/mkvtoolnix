#ifndef __MMG_QT_H
#define __MMG_QT_H

#include "config.h"

#define NAME "mmg/Qt"

#include <QString>

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

#endif  // __MMG_QT_H
