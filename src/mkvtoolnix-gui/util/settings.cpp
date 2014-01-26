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
  m_mkvmergeExe   = reg.value("mkvmergeExe", "mkvmerge").toString();
  m_priority      = static_cast<process_priority_e>(reg.value("priority", static_cast<int>(priority_normal)).toInt());
  m_lastOpenDir   = QDir{reg.value("lastOpenDir").toString()};
  m_lastOutputDir = QDir{reg.value("lastOutputDir").toString()};
  m_lastConfigDir = QDir{reg.value("lastConfigDir").toString()};
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
  reg.setValue("mkvmergeExe",   m_mkvmergeExe);
  reg.setValue("priority",      static_cast<int>(m_priority));
  reg.setValue("lastOpenDir",   m_lastOpenDir.path());
  reg.setValue("lastOutputDir", m_lastOutputDir.path());
  reg.setValue("lastConfigDir", m_lastConfigDir.path());
  reg.endGroup();

  reg.beginGroup("features");
  reg.setValue("setAudioDelayFromFileName", m_setAudioDelayFromFileName);
  reg.setValue("disableAVCompression",      m_disableAVCompression);
  reg.endGroup();

  reg.beginGroup("defaults");
  reg.setValue("defaultTrackLanguage", m_defaultTrackLanguage);
  reg.endGroup();
}
