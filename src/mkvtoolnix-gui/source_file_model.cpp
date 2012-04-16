#include "common/common_pch.h"

#include "mkvtoolnix-gui/source_file_model.h"

#include <QFileInfo>

SourceFileModel::SourceFileModel(QObject *parent)
  : QAbstractItemModel{parent}
  , m_sourceFiles{nullptr}
{
  m_additionalPartIcon.addFile(":/icons/16x16/distribute-horizontal-margin.png");
  m_addedIcon.addFile(":/icons/16x16/distribute-horizontal-x.png");
  m_normalIcon.addFile(":/icons/16x16/distribute-vertical-page.png");
}

SourceFileModel::~SourceFileModel() {
}

void
SourceFileModel::setSourceFiles(QList<SourceFilePtr> &sourceFiles) {
  m_sourceFiles = &sourceFiles;
  reset();
}


QModelIndex
SourceFileModel::index(int row,
                       int column,
                       QModelIndex const &parent)
  const {
  if (!m_sourceFiles || (0 > row) || (0 > column))
    return QModelIndex{};

  auto parentFile = sourceFileFromIndex(parent);
  if (!parentFile)
    return row < m_sourceFiles->size() ? createIndex(row, column, m_sourceFiles->at(row).get()) : QModelIndex{};

  auto childFile = row <  parentFile->m_additionalParts.size()                                       ? parentFile->m_additionalParts[row].get()
                 : row < (parentFile->m_additionalParts.size() + parentFile->m_appendedFiles.size()) ? parentFile->m_appendedFiles[row - parentFile->m_additionalParts.size()].get()
                 :                                                                                     nullptr;

  if (!childFile)
    return QModelIndex{};

  return createIndex(row, column, childFile);
}

QModelIndex
SourceFileModel::parent(QModelIndex const &child)
  const {
  auto sourceFile = sourceFileFromIndex(child);
  if (!sourceFile)
    return QModelIndex{};

  auto parentSourceFile = sourceFile->m_appendedTo;
  if (!parentSourceFile) {
    for (int row = 0; m_sourceFiles->size() > row; ++row)
      if (m_sourceFiles->at(row).get() == sourceFile)
        return createIndex(row, 0, nullptr);
    return QModelIndex{};
  }

  for (int row = 0; parentSourceFile->m_additionalParts.size() > row; ++row)
    if (parentSourceFile->m_additionalParts[row].get() == sourceFile)
      return createIndex(row, 0, parentSourceFile);

  for (int row = 0; parentSourceFile->m_appendedFiles.size() > row; ++row)
    if (parentSourceFile->m_appendedFiles[row].get() == sourceFile)
      return createIndex(row + parentSourceFile->m_additionalParts.size(), 0, parentSourceFile);

  return QModelIndex{};
}

int
SourceFileModel::rowCount(QModelIndex const &parent)
  const {
  if (parent.column() > 0)
    return 0;
  if (!m_sourceFiles)
    return 0;
  auto parentSourceFile = sourceFileFromIndex(parent);
  if (!parentSourceFile)
    return m_sourceFiles->size();
  return parentSourceFile->m_additionalParts.size() + parentSourceFile->m_appendedFiles.size();
}

int
SourceFileModel::columnCount(QModelIndex const &)
  const {
  return NumberOfColumns;
}

QVariant
SourceFileModel::dataDecoration(QModelIndex const &index,
                                SourceFile *sourceFile)
  const {
  return FileNameColumn != index.column() ? QVariant{}
       : sourceFile->m_additionalPart     ? m_additionalPartIcon
       : sourceFile->m_appended           ? m_addedIcon
       :                                    m_normalIcon;
}

QVariant
SourceFileModel::dataDisplay(QModelIndex const &index,
                             SourceFile *sourceFile)
  const {
  QFileInfo info{sourceFile->m_fileName};
  if (FileNameColumn == index.column())
    return QFileInfo{sourceFile->m_fileName}.fileName();

  else if (ContainerColumn == index.column())
    return sourceFile->m_additionalPart ? QY("(additional part)") : sourceFile->m_container;

  else if (SizeColumn == index.column())
    return QFileInfo{sourceFile->m_fileName}.size();

  else if (DirectoryColumn == index.column())
    return QFileInfo{sourceFile->m_fileName}.filePath();

  else
    return QVariant{};
}

QVariant
SourceFileModel::data(QModelIndex const &index,
                      int role)
  const {
  if (role == Qt::TextAlignmentRole)
    return SizeColumn == index.column() ? Qt::AlignRight : Qt::AlignLeft;

  auto sourceFile = sourceFileFromIndex(index);
  if (!sourceFile)
    return QVariant{};

  if (role == Qt::DecorationRole)
    return dataDecoration(index, sourceFile);

  if (role == Qt::DisplayRole)
    return dataDisplay(index, sourceFile);

  return QVariant{};
}

QVariant
SourceFileModel::headerData(int section,
                            Qt::Orientation orientation,
                            int role)
  const {
  if (Qt::Horizontal != orientation)
    return QVariant{};

  if (Qt::DisplayRole == role)
    return FileNameColumn  == section ? QY("File name")
         : ContainerColumn == section ? QY("Container")
         : SizeColumn      == section ? QY("File size")
         : DirectoryColumn == section ? QY("Directory")
         :                               Q("INTERNAL ERROR");

  if (Qt::TextAlignmentRole == role)
    return SizeColumn == section ? Qt::AlignRight : Qt::AlignLeft;

  return QVariant{};
}

SourceFile *
SourceFileModel::sourceFileFromIndex(QModelIndex const &index)
  const {
  if (index.isValid())
    return static_cast<SourceFile *>(index.internalPointer());
  return nullptr;
}
