#include "common/common_pch.h"

#include "common/sorting.h"
#include "mkvtoolnix-gui/source_file_model.h"
#include "mkvtoolnix-gui/track_model.h"
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
  m_sourceFiles = &sourceFiles;
  reset();
}

QList<QStandardItem *>
SourceFileModel::createRow(SourceFile *sourceFile)
  const {
  auto items = QList<QStandardItem *>{};
  auto info  = QFileInfo{sourceFile->m_fileName};

  items << new QStandardItem{info.fileName()};
  items << new QStandardItem{sourceFile->m_additionalPart ? QY("(additional part)") : sourceFile->m_container};
  items << new QStandardItem{QString::number(info.size())};
  items << new QStandardItem{info.filePath()};

  items[0]->setData(QVariant::fromValue(sourceFile), Util::SourceFileRole);
  items[0]->setIcon(  sourceFile->m_additionalPart ? m_additionalPartIcon
                    : sourceFile->m_appended       ? m_addedIcon
                    :                                m_normalIcon);

  items[2]->setTextAlignment(Qt::AlignRight);

  return items;
}

SourceFile *
SourceFileModel::fromIndex(QModelIndex const &index) {
  if (index.isValid())
    return index.data(Util::SourceFileRole).value<SourceFile *>();
  return nullptr;
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
