#include "common/common_pch.h"

#include <QComboBox>
#include <QIcon>
#include <QList>
#include <QString>
#include <QTreeView>

#include "common/qt.h"
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
resizeViewColumnsToContents(QTreeView *view) {
  auto columnCount = view->model()->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    view->resizeColumnToContents(column);
}

QList<QModelIndex>
selectedIndexes(QAbstractItemView *view) {
  auto indexes  = QList<QModelIndex>{};
  auto indexMap = QMap<QModelIndex, bool>{};

  for (auto const &range : view->selectionModel()->selection())
    for (auto const &index : range.indexes())
      if (!indexMap[index]) {
        indexMap[index] = true;
        indexes << index;
      }

  brng::sort(indexes);

  return indexes;
}

void
withSelectedIndexes(QAbstractItemView *view,
                    std::function<void(QModelIndex const &)> worker) {
  for (auto const &range : view->selectionModel()->selection())
    for (auto const &index : range.indexes())
      worker(index);
}

}
