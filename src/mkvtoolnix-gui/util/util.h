#ifndef MTX_MKVTOOLNIX_GUI_UTIL_H
#define MTX_MKVTOOLNIX_GUI_UTIL_H

#include "common/common_pch.h"

#include <QDialogButtonBox>
#include <QList>

class QAbstractItemView;
class QComboBox;
class QIcon;
class QModelIndex;
class QTreeView;
class QString;
class QVariant;

namespace Util {

// Container stuff
template<typename Tstored, typename Tcontainer>
int
findPtr(Tstored *needle,
        Tcontainer const &haystack) {
  auto itr = brng::find_if(haystack, [&](std::shared_ptr<Tstored> const &cmp) { return cmp.get() == needle; });
  return haystack.end() == itr ? -1 : std::distance(haystack.begin(), itr);
}

// Miscellaneous widget stuff
QIcon loadIcon(QString const &name, QList<int> const &sizes);
bool setComboBoxIndexIf(QComboBox *comboBox, std::function<bool(QString const &, QVariant const &)> test);
void enableWidgets(QList<QWidget *> const &widgets, bool enable);
QPushButton *buttonForRole(QDialogButtonBox *box, QDialogButtonBox::ButtonRole role = QDialogButtonBox::AcceptRole);

// Model stuff
enum MtxGuiRoles {
  SourceFileRole = Qt::UserRole + 1,
  TrackRole,
  JobIdRole,
};

void resizeViewColumnsToContents(QTreeView *view);
void withSelectedIndexes(QAbstractItemView *view, std::function<void(QModelIndex const &)> worker);

// String stuff
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

QString joinSentences(QStringList const &sentences);

};

#endif  // MTX_MKVTOOLNIX_GUI_UTIL_H
