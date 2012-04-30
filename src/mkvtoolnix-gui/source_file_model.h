#ifndef MTX_MKVTOOLNIXGUI_SOURCE_FILE_MODEL_H
#define MTX_MKVTOOLNIXGUI_SOURCE_FILE_MODEL_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/source_file.h"

#include <QAbstractItemModel>
#include <QIcon>
#include <QList>

class SourceFileModel;
typedef std::shared_ptr<SourceFileModel> SourceFileModelPtr;

class TrackModel;

class SourceFileModel : public QAbstractItemModel {
  Q_OBJECT;

protected:
  static int const FileNameColumn  = 0;
  static int const ContainerColumn = 1;
  static int const SizeColumn      = 2;
  static int const DirectoryColumn = 3;
  static int const NumberOfColumns = 4;

  QList<SourceFilePtr> *m_sourceFiles;
  QIcon m_additionalPartIcon, m_addedIcon, m_normalIcon;
  TrackModel *m_tracksModel;

public:
  SourceFileModel(QObject *parent);
  virtual ~SourceFileModel();

  virtual void setSourceFiles(QList<SourceFilePtr> &sourceFiles);
  virtual void setTracksModel(TrackModel *tracksModel);
  virtual void addOrAppendFilesAndTracks(QModelIndex fileToAddToIdx, QList<SourceFilePtr> const &files, bool append);
  virtual void addAdditionalParts(QModelIndex fileToAddToIdx, QStringList fileNames);
  virtual void clear();

  virtual QModelIndex index(int row, int column, QModelIndex const &parent) const;
  virtual QModelIndex parent(QModelIndex const &child) const;

  virtual int rowCount(QModelIndex const &parent) const;
  virtual int columnCount(QModelIndex const &parent) const;

  virtual QVariant data(QModelIndex const &index, int role) const;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

protected:
  SourceFile *sourceFileFromIndex(QModelIndex const &index) const;

  QVariant dataDecoration(QModelIndex const &index, SourceFile *sourceFile) const;
  QVariant dataDisplay(QModelIndex const &index, SourceFile *sourceFile) const;

  virtual void addFilesAndTracks(QList<SourceFilePtr> const &files);
  virtual void appendFilesAndTracks(QModelIndex fileToAddToIdx, QList<SourceFilePtr> const &files);
};

#endif  // MTX_MKVTOOLNIXGUI_SOURCE_FILE_MODEL_H
