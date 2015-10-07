#include "common/common_pch.h"

#include "common/strings/formatting.h"
#include "common/qt.h"
#include "mkvtoolnix-gui/forms/select_playlist_dialog.h"
#include "mkvtoolnix-gui/merge_widget/select_playlist_dialog.h"
#include "mkvtoolnix-gui/merge_widget/track.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QPushButton>

class ScannedFileItem: public QTreeWidgetItem {
public:
  SourceFile const *m_file;

public:
  ScannedFileItem(SourceFile const &file, QStringList const &texts);
  virtual bool operator <(QTreeWidgetItem const &cmp) const;

public:
  static ScannedFileItem *create(SourceFile const &file);
};

ScannedFileItem::ScannedFileItem(SourceFile const &file,
                                 QStringList const &texts)
  : QTreeWidgetItem{texts}
  , m_file{&file}
{
}

bool
ScannedFileItem::operator <(QTreeWidgetItem const &cmp)
  const {
  auto otherFile = static_cast<ScannedFileItem const &>(cmp).m_file;
  auto column    = treeWidget()->sortColumn();

  return 0 == column ? m_file->m_fileName         < otherFile->m_fileName
       : 1 == column ? m_file->m_playlistDuration < otherFile->m_playlistDuration
       :               m_file->m_playlistSize     < otherFile->m_playlistSize;
}

ScannedFileItem *
ScannedFileItem::create(SourceFile const &scannedFile) {
  auto item = new ScannedFileItem{ scannedFile, QStringList{
      QFileInfo{scannedFile.m_fileName}.fileName(),
      to_qs(format_timecode(scannedFile.m_playlistDuration, 0)),
      to_qs(format_file_size(scannedFile.m_playlistSize)),
    }};

  item->setTextAlignment(1, Qt::AlignRight);
  item->setTextAlignment(2, Qt::AlignRight);

  return item;
}

// ------------------------------------------------------------

class TrackItem: public QTreeWidgetItem {
public:
  Track const *m_track;

public:
  TrackItem(Track const &track, QStringList const &texts);
  virtual bool operator <(QTreeWidgetItem const &cmp) const;

public:
  static TrackItem *create(Track const &track);
};

TrackItem::TrackItem(Track const &track,
                     QStringList const &texts)
  : QTreeWidgetItem{texts}
  , m_track{&track}
{
}

bool
TrackItem::operator <(QTreeWidgetItem const &cmp)
  const {
  auto otherTrack = static_cast<TrackItem const &>(cmp).m_track;
  auto column     = treeWidget()->sortColumn();

  return 1 == column                                     ? m_track->nameForType() <  otherTrack->nameForType()
       : 2 == column                                     ? m_track->m_codec       <  otherTrack->m_codec
       : 3 == column                                     ? m_track->m_language    <  otherTrack->m_language
       : (m_track->m_id >= 0) && (otherTrack->m_id >= 0) ? m_track->m_id          <  otherTrack->m_id
       : (m_track->m_id <  0) && (otherTrack->m_id <  0) ? m_track->m_codec       <  otherTrack->m_codec
       :                                                   m_track->m_id          >= 0;
}

TrackItem *
TrackItem::create(Track const &track) {
  auto item = new TrackItem{ track, QStringList{
      track.m_id >= 0 ? QString::number(track.m_id) : QString{},
      track.nameForType(),
      track.m_codec,
      track.m_language,
    }};

  item->setTextAlignment(0, Qt::AlignRight);

  return item;
}

// ------------------------------------------------------------

SelectPlaylistDialog::SelectPlaylistDialog(QWidget *parent,
                                           QList<SourceFilePtr> const &scannedFiles)
  : QDialog{parent}
  , ui{new Ui::SelectPlaylistDialog}
  , m_scannedFiles{scannedFiles}
{
  // Setup UI controls.
  ui->setupUi(this);

  setupUi();
}

SelectPlaylistDialog::~SelectPlaylistDialog() {
  delete ui;
}

void
SelectPlaylistDialog::setupUi() {
  auto items = QList<QTreeWidgetItem *>{};
  for (auto const &scannedFile : m_scannedFiles)
    items << ScannedFileItem::create(*scannedFile);

  ui->scannedFiles->insertTopLevelItems(0, items);
  ui->scannedFiles->setSortingEnabled(true);
  ui->scannedFiles->sortItems(1, Qt::DescendingOrder);

  Util::resizeViewColumnsToContents(ui->scannedFiles);

  auto okButton = Util::buttonForRole(ui->buttonBox, QDialogButtonBox::AcceptRole);
  okButton->setText(QY("&Add"));
  okButton->setDefault(true);
}

void
SelectPlaylistDialog::onScannedFileSelected(QTreeWidgetItem *current,
                                            QTreeWidgetItem *) {
  auto selectedItem = static_cast<ScannedFileItem *>(current);
  if (!selectedItem)
    return;

  auto const &file = *selectedItem->m_file;

  ui->duration->setText(to_qs(format_timecode(file.m_playlistDuration, 0)));
  ui->size->setText(to_qs(format_file_size(file.m_playlistSize)));
  ui->numberOfChapters->setText(QString::number(file.m_playlistChapters));

  ui->tracks->setSortingEnabled(false);
  ui->playlistItems->setSortingEnabled(false);

  ui->tracks->clear();
  ui->playlistItems->clear();

  auto newItems = QList<QTreeWidgetItem *>{};
  for (auto const &track : file.m_tracks)
    newItems << TrackItem::create(*track);

  ui->tracks->insertTopLevelItems(0, newItems);

  newItems.empty();
  for (auto const &playlistFile : file.m_playlistFiles)
    newItems << createPlaylistItemItem(playlistFile);

  ui->playlistItems->insertTopLevelItems(0, newItems);

  ui->tracks->setSortingEnabled(true);
  ui->tracks->sortItems(ui->tracks->sortColumn(), Qt::AscendingOrder);

  ui->playlistItems->setSortingEnabled(true);
  ui->playlistItems->sortItems(ui->playlistItems->sortColumn(), Qt::AscendingOrder);

  Util::resizeViewColumnsToContents(ui->tracks);
  Util::resizeViewColumnsToContents(ui->playlistItems);
}

QTreeWidgetItem *
SelectPlaylistDialog::createPlaylistItemItem(QFileInfo const &fileInfo) {
  auto item = new QTreeWidgetItem{ QStringList{
      fileInfo.fileName(),
      fileInfo.path(),
    }};

  return item;
}

SourceFilePtr
SelectPlaylistDialog::select() {
  if (exec() == QDialog::Rejected)
    return SourceFilePtr{};

  auto item = static_cast<ScannedFileItem *>(ui->scannedFiles->currentItem());
  if (!item)
    return SourceFilePtr{};

  auto idx = Util::findPtr(item->m_file, m_scannedFiles);
  return idx >= 0 ? m_scannedFiles[idx] : SourceFilePtr{};
}
