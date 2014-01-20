#ifndef MTX_MKVTOOLNIX_GUI_SETTINGS_H
#define MTX_MKVTOOLNIX_GUI_SETTINGS_H

#include "common/common_pch.h"

#include <QDir>
#include <QString>

class Settings: public QObject {
  Q_OBJECT;
public:
  enum process_priority_e {
    priority_lowest = 0,
    priority_low,
    priority_normal,
    priority_high,
    priority_highest,
  };

  QString m_mkvmergeExe;
  process_priority_e m_priority;
  QDir m_lastOpenDir, m_lastOutputDir, m_lastConfigDir;
  bool m_setAudioDelayFromFileName, m_disableAVCompression;

public:
  Settings();
  void load();
  void save() const;

public:
  static Settings s_settings;
  static Settings &get();
};

// extern Settings g_settings;

#endif  // MTX_MKVTOOLNIX_GUI_SETTINGS_H
