#include "common/common_pch.h"

#include <QComboBox>
#include <QIcon>
#include <QList>
#include <QString>
#include <QTreeView>

#include "common/qt.h"
#include "common/strings/editing.h"
#include "mkvtoolnix-gui/util/util.h"

namespace Util {

QIcon
loadIcon(QString const &name,
         QList<int> const &sizes) {
  QIcon icon;
  for (auto size : sizes)
    icon.addFile(QString{":/icons/%1x%1/%2"}.arg(size).arg(name));

  return icon;
}

bool
setComboBoxIndexIf(QComboBox *comboBox,
                   std::function<bool(QString const &, QVariant const &)> test) {
  auto count = comboBox->count();
  for (int idx = 0; count > idx; ++idx)
    if (test(comboBox->itemText(idx), comboBox->itemData(idx))) {
      comboBox->setCurrentIndex(idx);
      return true;
    }

  return false;
}

void
enableWidgets(QList<QWidget *> const &widgets,
              bool enable) {
  for (auto &widget : widgets)
    widget->setEnabled(enable);
}

void
resizeViewColumnsToContents(QTreeView *view) {
  auto columnCount = view->model()->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    view->resizeColumnToContents(column);
}

void
withSelectedIndexes(QAbstractItemView *view,
                    std::function<void(QModelIndex const &)> worker) {
  auto rowsSeen = QSet<int>{};
  for (auto const &range : view->selectionModel()->selection())
    for (auto const &index : range.indexes()) {
      auto row = index.row();
      if (rowsSeen.contains(row))
        continue;
      rowsSeen << row;
      worker(index.sibling(row, 0));
    }
}

static QString
escape_mkvtoolnix(QString const &source) {
  return to_qs(::escape(to_utf8(source)));
}

static QString
unescape_mkvtoolnix(QString const &source) {
  return to_qs(::unescape(to_utf8(source)));
}

static QString
escape_shell_unix(QString const &source) {
  if (!source.contains(QRegExp{"[^\\w%+,\\-./:=@]"}))
    return source;

  auto copy = source;
  // ' -> '\''
  copy.replace(QRegExp{"'"}, Q("'\\''"));

  copy = Q("'%1'").arg(copy);
  copy.replace(QRegExp{"^''"}, Q(""));
  copy.replace(QRegExp{"''$"}, Q(""));

  return copy;
}

static QString
escape_shell_windows(QString const &source) {
  if (!source.contains(QRegExp{"[\\w+,\\-./:=@]"}))
    return source;

  auto copy = QString{'"'};

  for (auto it = source.begin(), end = source.end() ; ; ++it) {
    QString::size_type numBackslashes = 0;

    while ((it != end) && (*it == QChar{'\\'})) {
      ++it;
      ++numBackslashes;
    }

    if (it == end) {
      copy += QString{numBackslashes * 2, QChar{'\\'}};
      break;

    } else if (*it == QChar{'"'})
      copy += QString{numBackslashes * 2 + 1, QChar{'\\'}} + *it;

    else
      copy += QString{numBackslashes, QChar{'\\'}} + *it;
  }

  copy += QChar{'"'};

  copy.replace(QRegExp{"([()%!^\"<>&|])"}, Q("^\\1"));

  return copy;
}

QString
escape(QString const &source,
       EscapeMode mode) {
  return EscapeMkvtoolnix == mode ? escape_mkvtoolnix(source)
       : EscapeShellUnix  == mode ? escape_shell_unix(source)
       :                            escape_shell_windows(source);
}

QString
unescape(QString const &source,
         EscapeMode mode) {
  assert(EscapeMkvtoolnix == mode);

  return unescape_mkvtoolnix(source);
}

QStringList
escape(QStringList const &source,
       EscapeMode mode) {
  auto escaped = QStringList{};
  for (auto const &string : source)
    escaped << escape(string, mode);

  return escaped;
}

QStringList
unescape(QStringList const &source,
         EscapeMode mode) {
  auto unescaped = QStringList{};
  for (auto const &string : source)
    unescaped << unescape(string, mode);

  return unescaped;
}

}
