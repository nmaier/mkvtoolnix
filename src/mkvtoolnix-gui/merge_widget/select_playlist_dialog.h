#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_SELECT_PLAYLIST_DIALOG_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_SELECT_PLAYLIST_DIALOG_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/source_file.h"

#include <QDialog>
#include <QList>

namespace Ui {
class SelectPlaylistDialog;
}

class QFileInfo;
class QTreeWidgetItem;
class Track;

class SelectPlaylistDialog : public QDialog {
  Q_OBJECT;
protected:
  Ui::SelectPlaylistDialog *ui;
  QList<SourceFilePtr> m_scannedFiles;

public:
  explicit SelectPlaylistDialog(QWidget *parent, QList<SourceFilePtr> const &scannedFiles);
  ~SelectPlaylistDialog();

  SourceFilePtr select();

protected slots:
  void onScannedFileSelected(QTreeWidgetItem *current, QTreeWidgetItem *);

protected:
  void setupUi();

  static QTreeWidgetItem *createPlaylistItemItem(QFileInfo const &fileInfo);
};

#endif // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_SELECT_PLAYLIST_DIALOG_H
