#ifndef MTX_MKVTOOLNIX_GUI_SOURCE_FILE_MODEL_H
#define MTX_MKVTOOLNIX_GUI_SOURCE_FILE_MODEL_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/source_file.h"

#include <QStandardItemModel>
#include <QIcon>
#include <QList>

class SourceFileModel;
typedef std::shared_ptr<SourceFileModel> SourceFileModelPtr;

class TrackModel;

class SourceFileModel : public QStandardItemModel {
  Q_OBJECT;

protected:
  QList<SourceFilePtr> *m_sourceFiles;
  QIcon m_additionalPartIcon, m_addedIcon, m_normalIcon;
  TrackModel *m_tracksModel;

public:
  SourceFileModel(QObject *parent);
  virtual ~SourceFileModel();

  virtual void setSourceFiles(QList<SourceFilePtr> &sourceFiles);
  virtual void setTracksModel(TrackModel *tracksModel);
  virtual void addOrAppendFilesAndTracks(QModelIndex const &fileToAddToIdx, QList<SourceFilePtr> const &files, bool append);
  virtual void addAdditionalParts(QModelIndex const &fileToAddToIdx, QStringList const &fileNames);

protected:
  virtual void addFilesAndTracks(QList<SourceFilePtr> const &files);
  virtual void appendFilesAndTracks(QModelIndex const &fileToAddToIdx, QList<SourceFilePtr> const &files);
  virtual QModelIndex toTopLevelIdx(QModelIndex const &idx) const;

  QList<QStandardItem *> createRow(SourceFile *sourceFile) const;

public:                         // static
  static SourceFile *fromIndex(QModelIndex const &index);
};

Q_DECLARE_METATYPE(SourceFile *)

#endif  // MTX_MKVTOOLNIX_GUI_SOURCE_FILE_MODEL_H
