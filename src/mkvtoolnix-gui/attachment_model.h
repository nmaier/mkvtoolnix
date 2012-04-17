#ifndef MTX_MKVTOOLNIXGUI_ATTACHMENT_MODEL_H
#define MTX_MKVTOOLNIXGUI_ATTACHMENT_MODEL_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/attachment.h"

#include <QAbstractItemModel>
#include <QItemSelection>
#include <QList>

class AttachmentModel;
typedef std::shared_ptr<AttachmentModel> AttachmentModelPtr;

class AttachmentModel : public QAbstractItemModel {
  Q_OBJECT;

protected:
  static int const NameColumn        = 0;
  static int const MIMETypeColumn    = 1;
  static int const DescriptionColumn = 2;
  static int const StyleColumn       = 3;
  static int const SourceFileColumn  = 4;
  static int const NumberOfColumns   = 5;

  QList<AttachmentPtr> *m_attachments;

  bool m_debug;

public:
  AttachmentModel(QObject *parent, QList<AttachmentPtr> &attachments);
  virtual ~AttachmentModel();

  virtual void setAttachments(QList<AttachmentPtr> &attachments);

  virtual QModelIndex index(int row, int column, QModelIndex const &parent) const;
  virtual QModelIndex parent(QModelIndex const &child) const;

  virtual int rowCount(QModelIndex const &parent) const;
  virtual int columnCount(QModelIndex const &parent) const;

  virtual QVariant data(QModelIndex const &index, int role) const;
  virtual QVariant headerData(int section, Qt::Orientation orientation, int role) const;

  virtual void attachmentUpdated(Attachment *attachment);

  virtual bool removeRows(int row, int count, QModelIndex const &parent = QModelIndex{});
  virtual void removeSelectedAttachments(QItemSelection const &selection);

  virtual void addAttachments(QList<AttachmentPtr> const &attachmentsToAdd);

protected:
  Attachment *attachmentFromIndex(QModelIndex const &index) const;

  QVariant dataDisplay(QModelIndex const &index, Attachment *attachment) const;
};

#endif  // MTX_MKVTOOLNIXGUI_ATTACHMENT_MODEL_H
