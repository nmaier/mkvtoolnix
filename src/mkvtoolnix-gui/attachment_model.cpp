#include "common/common_pch.h"

#include "mkvtoolnix-gui/attachment_model.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileInfo>

AttachmentModel::AttachmentModel(QObject *parent)
  : QAbstractItemModel(parent)
  , m_attachments(nullptr)
  , m_debug(debugging_requested("attachment_model"))
{
}

AttachmentModel::~AttachmentModel() {
}

void
AttachmentModel::setAttachments(QList<AttachmentPtr> &attachments) {
  m_attachments = &attachments;
  reset();
}


QModelIndex
AttachmentModel::index(int row,
                       int column,
                       QModelIndex const &)
  const {
  if (!m_attachments || (0 > row) || (0 > column))
    return QModelIndex{};

  return row < m_attachments->size() ? createIndex(row, column, m_attachments->at(row).get()) : QModelIndex{};
}

QModelIndex
AttachmentModel::parent(QModelIndex const &)
  const {
  return QModelIndex{};
}

int
AttachmentModel::rowCount(QModelIndex const &parent)
  const {
  if (parent.column() > 0)
    return 0;
  if (!m_attachments)
    return 0;
  auto parentAttachment = attachmentFromIndex(parent);
  if (!parentAttachment)
    return m_attachments->size();
  return 0;
}

int
AttachmentModel::columnCount(QModelIndex const &)
  const {
  return NumberOfColumns;
}

QVariant
AttachmentModel::dataDisplay(QModelIndex const &index,
                             Attachment *attachment)
  const {
  if (NameColumn == index.column())
    return attachment->m_name;

  else if (MIMETypeColumn == index.column())
    return attachment->m_MIMEType;

  else if (DescriptionColumn == index.column())
    return attachment->m_description;

  else if (StyleColumn == index.column())
    return attachment->m_style == Attachment::ToAllFiles ? QY("to all files") : QY("only to the first file");

  else if (SourceFileColumn == index.column())
    return attachment->m_fileName;

  else
    return QVariant{};
}

QVariant
AttachmentModel::data(QModelIndex const &index,
                      int role)
  const {
  auto attachment = attachmentFromIndex(index);
  if (!attachment)
    return QVariant{};

  if (Qt::TextAlignmentRole == role)
    return Qt::AlignLeft;

  if (Qt::DisplayRole == role)
    return dataDisplay(index, attachment);

  return QVariant{};
}

QVariant
AttachmentModel::headerData(int section,
                            Qt::Orientation orientation,
                            int role)
  const {
  if (Qt::Horizontal != orientation)
    return QVariant{};

  if (Qt::DisplayRole == role)
    return NameColumn        == section ? QY("Name")
         : MIMETypeColumn    == section ? QY("MIME type")
         : DescriptionColumn == section ? QY("Description")
         : SourceFileColumn  == section ? QY("Source file")
         : StyleColumn       == section ? QY("Attach to")
         : SourceFileColumn  == section ? QY("Source file name")
         :                                Q("INTERNAL ERROR");

  if (Qt::TextAlignmentRole == role)
    return Qt::AlignLeft;

  return QVariant{};
}

Attachment *
AttachmentModel::attachmentFromIndex(QModelIndex const &index)
  const {
  if (index.isValid())
    return static_cast<Attachment *>(index.internalPointer());
  return nullptr;
}

void
AttachmentModel::attachmentUpdated(Attachment *attachment) {
  if (nullptr == m_attachments) {
    mxdebug_if(m_debug, boost::format("attachmentUpdated() called but m_attachments == nullptr!?\n"));
    return;
  }

  auto it = brng::find_if(*m_attachments, [&](AttachmentPtr const &candidate) { return candidate.get() == attachment; });
  if (it == m_attachments->end())
    return;

  int row = std::distance(m_attachments->begin(), it);
  emit dataChanged(createIndex(row, 0, attachment), createIndex(row, NumberOfColumns - 1, attachment));
}

void
AttachmentModel::removeAttachments(QList<Attachment *> const &toRemove) {
  if ((nullptr == m_attachments) || toRemove.isEmpty())
    return;

  beginResetModel();

  for (int idx = m_attachments->size() - 1; 0 <= idx; --idx)
    if (toRemove.contains(m_attachments->at(idx).get()))
      m_attachments->removeAt(idx);

  endResetModel();
}
