#ifndef MTX_MMGQT_FILE_TYPE_FILTER_H
#define MTX_MMGQT_FILE_TYPE_FILTER_H

#include "common/common_pch.h"

#include <QStringList>

class FileTypeFilter {
public:
  static QStringList const & get();

public:
  static QStringList s_filter;
};

#endif // MTX_MMGQT_FILE_TYPE_FILTER_H
