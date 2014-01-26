#ifndef MTX_MKVTOOLNIX_GUI_UTIL_H
#define MTX_MKVTOOLNIX_GUI_UTIL_H

#include "common/common_pch.h"

#include <QList>

class QAbstractItemView;
class QComboBox;
class QIcon;
class QModelIndex;
class QTreeView;
class QString;
class QVariant;

namespace Util {

enum MtxGuiRoles {
  SourceFileRole = Qt::UserRole + 1,
  TrackRole,
  JobIdRole,
};

QIcon loadIcon(QString const &name, QList<int> const &sizes);
bool setComboBoxIndexIf(QComboBox *comboBox, std::function<bool(QString const &, QVariant const &)> test);
void enableWidgets(QList<QWidget *> const &widgets, bool enable);
void resizeViewColumnsToContents(QTreeView *view);
QList<QModelIndex> selectedIndexes(QAbstractItemView *view);
void withSelectedIndexes(QAbstractItemView *view, std::function<void(QModelIndex const &)> worker);

enum EscapeMode {
  EscapeMkvtoolnix,
  EscapeShellUnix,
  EscapeShellWindows,
#if defined(SYS_WINDOWS)
  EscapeShellNative = EscapeShellWindows,
#else
  EscapeShellNative = EscapeShellUnix,
#endif
};

QString escape(QString const &source, EscapeMode mode);
QStringList escape(QStringList const &source, EscapeMode mode);
QString unescape(QString const &source, EscapeMode mode);
QStringList unescape(QStringList const &source, EscapeMode mode);

};

#endif  // MTX_MKVTOOLNIX_GUI_UTIL_H
