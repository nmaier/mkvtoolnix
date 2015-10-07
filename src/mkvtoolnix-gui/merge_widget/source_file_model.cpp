#include "common/common_pch.h"

#include "common/sorting.h"
#include "common/strings/formatting.h"
#include "mkvtoolnix-gui/merge_widget/source_file_model.h"
#include "mkvtoolnix-gui/merge_widget/track_model.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileInfo>

SourceFileModel::SourceFileModel(QObject *parent)
  : QStandardItemModel{parent}
  , m_sourceFiles{}
  , m_tracksModel{}
{
  m_additionalPartIcon.addFile(":/icons/16x16/distribute-horizontal-margin.png");
  m_addedIcon.addFile(":/icons/16x16/distribute-horizontal-x.png");
  m_normalIcon.addFile(":/icons/16x16/distribute-vertical-page.png");

  auto labels = QStringList{};
  labels << QY("File name") << QY("Container") << QY("File size") << QY("Directory");
  setHorizontalHeaderLabels(labels);
  horizontalHeaderItem(2)->setTextAlignment(Qt::AlignRight);
}

SourceFileModel::~SourceFileModel() {
}

void
SourceFileModel::setTracksModel(TrackModel *tracksModel) {
  m_tracksModel = tracksModel;
}

void
SourceFileModel::setSourceFiles(QList<SourceFilePtr> &sourceFiles) {
  removeRows(0, rowCount());

  m_sourceFiles = &sourceFiles;
  auto row      = 0u;

  for (auto const &file : *m_sourceFiles) {
    invisibleRootItem()->appendRow(createRow(file.get()));

    for (auto const &additionalPart : file->m_additionalParts)
      item(row)->appendRow(createRow(additionalPart.get()));

    for (auto const &appendedFile : file->m_appendedFiles)
      item(row)->appendRow(createRow(appendedFile.get()));

    ++row;
  }
}

QList<QStandardItem *>
SourceFileModel::createRow(SourceFile *sourceFile)
  const {
  auto items = QList<QStandardItem *>{};
  auto info  = QFileInfo{sourceFile->m_fileName};

  items << new QStandardItem{info.fileName()};
  items << new QStandardItem{sourceFile->isAdditionalPart() ? QY("(additional part)") : sourceFile->m_container};
  items << new QStandardItem{to_qs(format_file_size(sourceFile->isPlaylist() ? sourceFile->m_playlistSize : info.size()))};
  items << new QStandardItem{info.path()};

  items[0]->setData(QVariant::fromValue(sourceFile), Util::SourceFileRole);
  items[0]->setIcon(  sourceFile->isAdditionalPart() ? m_additionalPartIcon
                    : sourceFile->isAppended()       ? m_addedIcon
                    :                                  m_normalIcon);

  items[2]->setTextAlignment(Qt::AlignRight);

  return items;
}

SourceFile *
SourceFileModel::fromIndex(QModelIndex const &idx) {
  if (!idx.isValid())
    return nullptr;
  return index(idx.row(), 0, idx.parent())
    .data(Util::SourceFileRole)
    .value<SourceFile *>();
}

void
SourceFileModel::addAdditionalParts(QModelIndex const &fileToAddToIdx,
                                    QStringList const &fileNames) {
  auto actualIdx = toTopLevelIdx(fileToAddToIdx);
  if (fileNames.isEmpty() || !actualIdx.isValid())
    return;

  auto fileToAddTo = fromIndex(actualIdx);
  auto itemToAddTo = itemFromIndex(actualIdx);
  assert(fileToAddTo && itemToAddTo);

  auto actualFileNames = QStringList{};
  std::copy_if(fileNames.begin(), fileNames.end(), std::back_inserter(actualFileNames), [&fileToAddTo](QString const &fileName) -> bool {
      if (fileToAddTo->m_fileName == fileName)
        return false;
      for (auto additionalPart : fileToAddTo->m_additionalParts)
        if (additionalPart->m_fileName == fileName)
          return false;
      return true;
    });

  if (actualFileNames.isEmpty())
    return;

  mtx::sort::naturally(actualFileNames.begin(), actualFileNames.end());

  for (auto &fileName : actualFileNames) {
    auto additionalPart              = std::make_shared<SourceFile>(fileName);
    additionalPart->m_additionalPart = true;
    additionalPart->m_appendedTo     = fileToAddTo;

    fileToAddTo->m_additionalParts << additionalPart;
    itemToAddTo->appendRow(createRow(additionalPart.get()));
  }
}

void
SourceFileModel::addOrAppendFilesAndTracks(QModelIndex const &fileToAddToIdx,
                                           QList<SourceFilePtr> const &files,
                                           bool append) {
  assert(m_tracksModel);

  if (files.isEmpty())
    return;

  if (append)
    appendFilesAndTracks(fileToAddToIdx, files);
  else
    addFilesAndTracks(files);
}

void
SourceFileModel::addFilesAndTracks(QList<SourceFilePtr> const &files) {
  *m_sourceFiles << files;
  for (auto const &file : files)
    invisibleRootItem()->appendRow(createRow(file.get()));

  m_tracksModel->addTracks(std::accumulate(files.begin(), files.end(), QList<TrackPtr>{}, [](QList<TrackPtr> &accu, SourceFilePtr const &file) { return accu << file->m_tracks; }));
}

void
SourceFileModel::removeFile(SourceFile *fileToBeRemoved) {
  if (fileToBeRemoved->isAdditionalPart()) {
    auto row           = Util::findPtr(fileToBeRemoved,               fileToBeRemoved->m_appendedTo->m_additionalParts);
    auto parentFileRow = Util::findPtr(fileToBeRemoved->m_appendedTo, *m_sourceFiles);

    assert((-1 != row) && (-1 != parentFileRow));

    item(parentFileRow)->removeRow(row);
    fileToBeRemoved->m_appendedTo->m_additionalParts.removeAt(row);

    return;
  }

  if (fileToBeRemoved->isAppended()) {
    auto row           = Util::findPtr(fileToBeRemoved,               fileToBeRemoved->m_appendedTo->m_appendedFiles);
    auto parentFileRow = Util::findPtr(fileToBeRemoved->m_appendedTo, *m_sourceFiles);

    assert((-1 != row) && (-1 != parentFileRow));

    item(parentFileRow)->removeRow(row);
    fileToBeRemoved->m_appendedTo->m_appendedFiles.removeAt(row);

    return;
  }

  auto row = Util::findPtr(fileToBeRemoved, *m_sourceFiles);
  assert(-1 != row);

  invisibleRootItem()->removeRow(row);
  m_sourceFiles->removeAt(row);
}

void
SourceFileModel::removeFiles(QList<SourceFile *> const &files) {
  auto filesToRemove  = files.toSet();
  auto tracksToRemove = QSet<Track *>{};

  mxinfo(boost::format("about to remove\n"));

  for (auto const &file : files) {
    for (auto const &track : file->m_tracks)
      tracksToRemove << track.get();

    for (auto const &appendedFile : file->m_appendedFiles) {
      filesToRemove << appendedFile.get();
      for (auto const &track : appendedFile->m_tracks)
        tracksToRemove << track.get();
    }
  }

  // TODO: SourceFileModel::removeFiles re-distribute orphaned tracks

  m_tracksModel->removeTracks(tracksToRemove);

  auto filesToRemoveLast = QList<SourceFile *>{};
  for (auto &file : filesToRemove)
    if (!file->isRegular())
      removeFile(file);
    else
      filesToRemoveLast << file;

  for (auto &file : filesToRemoveLast)
    if (file->isRegular())
      removeFile(file);
}

QModelIndex
SourceFileModel::toTopLevelIdx(QModelIndex const &idx)
  const {
  if (!idx.isValid())
    return QModelIndex{};

  auto parent = idx.parent();
  return parent == QModelIndex{} ? idx : parent;
}

void
SourceFileModel::appendFilesAndTracks(QModelIndex const &fileToAppendToIdx,
                                      QList<SourceFilePtr> const &files) {
  auto actualIdx = toTopLevelIdx(fileToAppendToIdx);
  if (files.isEmpty() || !actualIdx.isValid())
    return;

  auto fileToAppendTo = fromIndex(actualIdx);
  auto itemToAppendTo = itemFromIndex(actualIdx);
  assert(fileToAppendTo && itemToAppendTo);

  for (auto const &file : files) {
    file->m_appended   = true;
    file->m_appendedTo = fileToAppendTo;

    fileToAppendTo->m_appendedFiles << file;
    itemToAppendTo->appendRow(createRow(file.get()));
  }

  for (auto const &file : files)
    m_tracksModel->appendTracks(fileToAppendTo, file->m_tracks);
}
