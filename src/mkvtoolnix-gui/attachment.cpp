#include "common/common_pch.h"

#include "common/extern_data.h"
#include "mkvtoolnix-gui/mux_config.h"
#include "mkvtoolnix-gui/source_file.h"
#include "mkvtoolnix-gui/attachment.h"
#include "mkvtoolnix-gui/util/settings.h"

#include <QFileInfo>

Attachment::Attachment(QString const &fileName)
  : m_fileName(fileName)
  , m_name(QFileInfo{fileName}.fileName())
  , m_style(ToAllFiles)
{
}

Attachment::~Attachment() {
}

void
Attachment::saveSettings(QSettings &settings)
  const {
  settings.setValue("fileName",    m_fileName);
  settings.setValue("name",        m_name);
  settings.setValue("description", m_description);
  settings.setValue("MIMEType",    m_MIMEType);
  settings.setValue("style",       static_cast<int>(m_style));
}

void
Attachment::guessMIMEType() {
  m_MIMEType = to_qs(guess_mime_type(to_utf8(m_fileName), true));
}
