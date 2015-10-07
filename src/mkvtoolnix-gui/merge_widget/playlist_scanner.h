#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_PLAYLIST_SCANNER_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_PLAYLIST_SCANNER_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/source_file.h"

#include <QList>

class QWidget;

class PlaylistScanner {
protected:
  QWidget *m_parent;

public:
  explicit PlaylistScanner(QWidget *parent);

  void checkAddingPlaylists(QList<SourceFilePtr> &files);

protected:
  bool askScanForPlaylists(SourceFile const &file, unsigned int numOtherFiles);
  QList<SourceFilePtr> scanForPlaylists(QFileInfoList const &otherFiles);
};

#endif // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_PLAYLIST_SCANNER_H
