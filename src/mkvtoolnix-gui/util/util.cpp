#include "common/common_pch.h"

#include <QIcon>
#include <QList>
#include <QString>

#include "common/qt.h"
#include "mkvtoolnix-gui/util/util.h"

QIcon
Util::loadIcon(QString const &name,
               QList<int> const &sizes) {
  QIcon icon;
  for (auto size : sizes)
    icon.addFile(QString{":/icons/%1x%1/%2"}.arg(size).arg(name));

  return icon;
}
