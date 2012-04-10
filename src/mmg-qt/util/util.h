#ifndef MTX_MMGQT_UTIL_H
#define MTX_MMGQT_UTIL_H

#include "common/common_pch.h"

class QIcon;
class QString;
template<typename T> class QList;

class Util {
public:
  static QIcon loadIcon(QString const &name, QList<int> const &sizes);
};

#endif  // MTX_MMGQT_UTIL_H
