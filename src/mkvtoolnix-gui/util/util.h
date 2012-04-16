#ifndef MTX_MMGQT_UTIL_H
#define MTX_MMGQT_UTIL_H

#include "common/common_pch.h"

#include <QComboBox>
#include <QIcon>
#include <QList>
#include <QString>

class Util {
public:
  static QIcon loadIcon(QString const &name, QList<int> const &sizes);
  static bool setComboBoxIndexIf(QComboBox *comboBox, std::function<bool(QString const &, QVariant const &)> test);
};

#endif  // MTX_MMGQT_UTIL_H
