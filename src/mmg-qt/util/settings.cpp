#include "common/common_pch.h"

#include "mmg-qt/util/settings.h"

Settings Settings::s_settings;

Settings::Settings()
  : m_mkvmergeExe("mkvmerge")
  , m_priority(priority_normal)
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
