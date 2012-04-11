#ifndef MTX_MKVTOOLNIXGUI_SOURCE_FILE_MODEL_H
#define MTX_MKVTOOLNIXGUI_SOURCE_FILE_MODEL_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/source_file.h"

#include <QAbstractItemModel>
#include <QIcon>
#include <QList>

class SourceFileModel;
typedef std::shared_ptr<SourceFileModel> SourceFileModelPtr;

class SourceFileModel : public QAbstractItemModel {
  Q_OBJECT;

protected:
  QList<SourceFilePtr> *m_sourceFiles;
  QIcon m_additionalPartIcon, m_addedIcon, m_normalIcon;

public:
  SourceFileModel(QObject *parent);
  virtual ~SourceFileModel();

  virtual void setSourceFiles(QList<SourceFilePtr> &sourceFiles);

  virtual QModelIndex index(int row, int column, QModelIndex const &parent) const;
  virtual QModelIndex parent(QModelIndex const &child) const;

  int rowCount(QModelIndex const &parent) const;
  int columnCount(QModelIndex const &parent) const;

  QVariant data(QModelIndex const &index, int role) const;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const;

protected:
  SourceFile *sourceFileFromIndex(QModelIndex const &index) const;
};

#endif  // MTX_MKVTOOLNIXGUI_SOURCE_FILE_MODEL_H
