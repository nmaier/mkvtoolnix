#include "common/common_pch.h"

#include "mkvtoolnix-gui/util/settings.h"

Settings Settings::s_settings;

Settings::Settings()
  : m_mkvmergeExe("mkvmerge")
  , m_priority(priority_normal)
  , m_setAudioDelayFromFileName(true)
  , m_disableAVCompression(true)
{
}

Settings &
Settings::get() {
  return s_settings;
}

void
Settings::load() {
}

void
Settings::save() {
}
