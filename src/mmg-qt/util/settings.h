#ifndef MTX_MMGQT_SETTINGS_H
#define MTX_MMGQT_SETTINGS_H

#include "common/common_pch.h"

#include <QDir>
#include <QString>

class Settings: public QObject {
  Q_OBJECT;
public:
  enum process_priority_e {
    priority_lowest,
    priority_low,
    priority_normal,
    priority_high,
    priority_highest,
  };

  QString m_mkvmergeExe;
  process_priority_e m_priority;
  QDir m_lastOpenDir;

public:
  Settings();
  void load();
  void save();

public:
  static Settings s_settings;
  static Settings &get();
};

// extern Settings g_settings;

#endif  // MTX_MMGQT_SETTINGS_H
