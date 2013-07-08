#ifndef MTX_MMGQT_UTIL_H
#define MTX_MMGQT_UTIL_H

#include "common/common_pch.h"

#include <QComboBox>
#include <QIcon>
#include <QList>
#include <QString>

namespace Util {

enum MtxGuiRoles {
  SourceFileRole = Qt::UserRole + 1,
  TrackRole
};

QIcon loadIcon(QString const &name, QList<int> const &sizes);
bool setComboBoxIndexIf(QComboBox *comboBox, std::function<bool(QString const &, QVariant const &)> test);

};

#endif  // MTX_MMGQT_UTIL_H
