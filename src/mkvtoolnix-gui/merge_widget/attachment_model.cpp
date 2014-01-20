#include "common/common_pch.h"

#include "mkvtoolnix-gui/merge_widget/attachment_model.h"
#include "mkvtoolnix-gui/util/util.h"

#include <boost/range/adaptor/uniqued.hpp>
#include <QFileInfo>

AttachmentModel::AttachmentModel(QObject *parent,
                                 QList<AttachmentPtr> &attachments)
  : QAbstractItemModel(parent)
  , m_attachments(&attachments)
  , m_debug{"attachment_model"}
{
}

AttachmentModel::~AttachmentModel() {
}

void
AttachmentModel::setAttachments(QList<AttachmentPtr> &attachments) {
  beginResetModel();
  m_attachments = &attachments;
  endResetModel();
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
  if (!m_attachments) {
    mxdebug_if(m_debug, boost::format("attachmentUpdated() called but !m_attachments!?\n"));
    return;
  }

  auto it = brng::find_if(*m_attachments, [&](AttachmentPtr const &candidate) { return candidate.get() == attachment; });
  if (it == m_attachments->end())
    return;

  int row = std::distance(m_attachments->begin(), it);
  emit dataChanged(createIndex(row, 0, attachment), createIndex(row, NumberOfColumns - 1, attachment));
}

void
AttachmentModel::removeSelectedAttachments(QItemSelection const &selection) {
  if (selection.isEmpty())
    return;

  std::vector<int> rowsToRemove;

  for (auto &range : selection)
    for (auto &index : range.indexes())
      rowsToRemove.push_back(index.row());

  brng::sort(rowsToRemove, std::greater<int>());

  for (auto row : rowsToRemove | badap::uniqued)
    removeRow(row);
}

bool
AttachmentModel::removeRows(int row,
                            int count,
                            QModelIndex const &parent) {
  if ((0 > row) || (0 >= count) || ((row + count) > m_attachments->size()))
    return false;

  beginRemoveRows(parent, row, row + count);

  m_attachments->erase(m_attachments->begin() + row, m_attachments->begin() + row + count);

  endRemoveRows();

  return true;
}

void
AttachmentModel::addAttachments(QList<AttachmentPtr> const &attachmentsToAdd) {
  if (attachmentsToAdd.isEmpty())
    return;

  beginInsertRows(QModelIndex{}, m_attachments->size(), m_attachments->size() + attachmentsToAdd.size() - 1);

  *m_attachments << attachmentsToAdd;

  endInsertRows();
}
