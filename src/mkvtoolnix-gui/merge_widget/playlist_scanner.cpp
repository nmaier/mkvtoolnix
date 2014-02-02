#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/ask_scan_for_playlists_dialog.h"
#include "mkvtoolnix-gui/merge_widget/playlist_scanner.h"
#include "mkvtoolnix-gui/merge_widget/select_playlist_dialog.h"
#include "mkvtoolnix-gui/util/file_identifier.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QProgressDialog>
#include <QString>

PlaylistScanner::PlaylistScanner(QWidget *parent)
  : m_parent{parent}
{
}

void
PlaylistScanner::checkAddingPlaylists(QList<SourceFilePtr> &files) {
  if (Settings::NeverScan == Settings::get().m_scanForPlaylistsPolicy)
    return;

  for (auto &file : files) {
    if (!file->isPlaylist())
      continue;

    auto info       = QFileInfo{file->m_fileName};
    auto otherFiles = QDir{info.path()}.entryInfoList(QStringList{QString{"*.%1"}.arg(info.suffix())}, QDir::Files, QDir::Name);
    if (otherFiles.isEmpty())
      continue;

    auto doScan = Settings::AlwaysScan == Settings::get().m_scanForPlaylistsPolicy;
    if (!doScan)
      doScan = askScanForPlaylists(*file, otherFiles.size());

    if (!doScan)
      continue;

    auto identifiedFiles = scanForPlaylists(otherFiles);
    if (identifiedFiles.isEmpty())
      continue;

    else if (1 == identifiedFiles.size())
      file = identifiedFiles[0];

    else {
      auto selectedFile = SelectPlaylistDialog{m_parent, identifiedFiles}.select();
      if (selectedFile)
        file = selectedFile;
    }
  }
}

bool
PlaylistScanner::askScanForPlaylists(SourceFile const &file,
                                 unsigned int numOtherFiles) {

  AskScanForPlaylistsDialog dialog{m_parent};
  return dialog.ask(file, numOtherFiles);
}

QList<SourceFilePtr>
PlaylistScanner::scanForPlaylists(QFileInfoList const &otherFiles) {
  QProgressDialog progress{ QY("Scanning directory"), QY("Cancel"), 0, otherFiles.size(), m_parent };
  progress.setWindowModality(Qt::ApplicationModal);

  auto identifiedFiles = QList<SourceFilePtr>{};
  auto numScanned      = 0u;

  for (auto const &otherFile : otherFiles) {
    progress.setLabelText(QNY("%1 of %2 file processed", "%1 of %2 files processed", otherFiles.size()).arg(numScanned).arg(otherFiles.size()));
    progress.setValue(numScanned++);

    qApp->processEvents();
    if (progress.wasCanceled())
      return QList<SourceFilePtr>{};

    FileIdentifier identifier{m_parent, otherFile.filePath()};
    if (!identifier.identify())
      continue;

    auto file = identifier.file();
    if (file->isPlaylist() && (file->m_playlistDuration >= (Settings::get().m_minimumPlaylistDuration * 1000000000ull)))
      identifiedFiles << file;
  }

  progress.setValue(otherFiles.size());

  std::sort(identifiedFiles.begin(), identifiedFiles.end(), [](SourceFilePtr const &a, SourceFilePtr const &b) { return a->m_fileName < b->m_fileName; });

  return identifiedFiles;
}
