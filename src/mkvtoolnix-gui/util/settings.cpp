#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/util/settings.h"

#include <QSettings>

Settings Settings::s_settings;

Settings::Settings()
{
  load();
}

Settings &
Settings::get() {
  return s_settings;
}

void
Settings::load() {
  QSettings reg;

  reg.beginGroup("settings");
  m_mkvmergeExe             = reg.value("mkvmergeExe", "mkvmerge").toString();
  m_priority                = static_cast<ProcessPriority>(reg.value("priority", static_cast<int>(NormalPriority)).toInt());
  m_lastOpenDir             = QDir{reg.value("lastOpenDir").toString()};
  m_lastOutputDir           = QDir{reg.value("lastOutputDir").toString()};
  m_lastConfigDir           = QDir{reg.value("lastConfigDir").toString()};
  m_scanForPlaylistsPolicy  = static_cast<ScanForPlaylistsPolicy>(reg.value("scanForPlaylistsPolicy", static_cast<int>(AskBeforeScanning)).toInt());
  m_minimumPlaylistDuration = reg.value("minimumPlaylistDuration", 120).toUInt();
  reg.endGroup();

  reg.beginGroup("features");
  m_setAudioDelayFromFileName = reg.value("setAudioDelayFromFileName", true).toBool();
  m_disableAVCompression      = reg.value("disableAVCompression",      false).toBool();
  reg.endGroup();

  reg.beginGroup("defaults");
  m_defaultTrackLanguage = reg.value("defaultTrackLanguage", Q("und")).toString();
  reg.endGroup();
}

void
Settings::save()
  const {
  QSettings reg;

  reg.beginGroup("settings");
  reg.setValue("mkvmergeExe",             m_mkvmergeExe);
  reg.setValue("priority",                static_cast<int>(m_priority));
  reg.setValue("lastOpenDir",             m_lastOpenDir.path());
  reg.setValue("lastOutputDir",           m_lastOutputDir.path());
  reg.setValue("lastConfigDir",           m_lastConfigDir.path());
  reg.setValue("scanForPlaylistsPolicy",  static_cast<int>(m_scanForPlaylistsPolicy));
  reg.setValue("minimumPlaylistDuration", m_minimumPlaylistDuration);
  reg.endGroup();

  reg.beginGroup("features");
  reg.setValue("setAudioDelayFromFileName", m_setAudioDelayFromFileName);
  reg.setValue("disableAVCompression",      m_disableAVCompression);
  reg.endGroup();

  reg.beginGroup("defaults");
  reg.setValue("defaultTrackLanguage", m_defaultTrackLanguage);
  reg.endGroup();
}
