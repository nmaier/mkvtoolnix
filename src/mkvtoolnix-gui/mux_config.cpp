#include "common/common_pch.h"

#include "mkvtoolnix-gui/mux_config.h"

MuxConfig::MuxConfig()
  : m_splitMode{DoNotSplit}
  , m_splitMaxFiles{0}
  , m_linkFiles{false}
  , m_webmMode{false}
{
}

MuxConfig::~MuxConfig()
{
}

void
MuxConfig::load(QSettings const &settings) {
  // TODO
}

void
MuxConfig::save(QSettings &settings)
  const {
  // TODO
}

void
MuxConfig::save(QString const &fileName)
  const {
  // TODO
}

void
MuxConfig::reset()
  const {
  // TODO
}

MuxConfigPtr
MuxConfig::load(QString const &fileName) {
  auto config = std::make_shared<MuxConfig>();
  config->load(QSettings{fileName, QSettings::IniFormat});

  return config;
}
