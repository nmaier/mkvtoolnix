#ifndef MTX_MKVTOOLNIX_GUI_SETTINGS_H
#define MTX_MKVTOOLNIX_GUI_SETTINGS_H

#include "common/common_pch.h"

#include <QDir>
#include <QString>

class Settings: public QObject {
  Q_OBJECT;
public:
  enum ProcessPriority {
    LowestPriority = 0,
    LowPriority,
    NormalPriority,
    HighPriority,
    HighestPriority,
  };

  enum ScanForPlaylistsPolicy {
    AskBeforeScanning = 0,
    AlwaysScan,
    NeverScan,
  };

  QString m_mkvmergeExe, m_defaultTrackLanguage;
  ProcessPriority m_priority;
  QDir m_lastOpenDir, m_lastOutputDir, m_lastConfigDir;
  bool m_setAudioDelayFromFileName, m_disableAVCompression;

  ScanForPlaylistsPolicy m_scanForPlaylistsPolicy;
  unsigned int m_minimumPlaylistDuration;

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
