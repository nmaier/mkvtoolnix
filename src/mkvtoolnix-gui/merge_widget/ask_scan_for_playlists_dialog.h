#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_ASK_SCAN_FOR_PLAYLISTS_DIALOG_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_ASK_SCAN_FOR_PLAYLISTS_DIALOG_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/source_file.h"

#include <QDialog>

namespace Ui {
class AskScanForPlaylistsDialog;
}

class AskScanForPlaylistsDialog : public QDialog {
  Q_OBJECT;
protected:
  Ui::AskScanForPlaylistsDialog *ui;

public:
  explicit AskScanForPlaylistsDialog(QWidget *parent = nullptr);
  ~AskScanForPlaylistsDialog();

  bool ask(SourceFile const &file, unsigned int otherFiles);
};

#endif // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_ASK_SCAN_FOR_PLAYLISTS_DIALOG_H
