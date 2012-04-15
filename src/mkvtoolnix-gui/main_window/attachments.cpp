#include "common/common_pch.h"

#include "common/extern_data.h"
#include "common/qt.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/forms/main_window.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>

void
MainWindow::setupAttachmentsControls() {
  ui->attachments->setModel(m_attachmentsModel);
  ui->splitMaxFiles->setMaximum(std::numeric_limits<int>::max());

  // MIME type
  for (auto &mime_type : mime_types)
    ui->attachmentMIMEType->addItem(to_qs(mime_type.name), to_qs(mime_type.name));

  // Attachment style
  ui->attachmentStyle->setItemData(0, static_cast<int>(Attachment::ToAllFiles));
  ui->attachmentStyle->setItemData(1, static_cast<int>(Attachment::ToFirstFile));

  // Context menu
  m_addAttachmentsAction    = new QAction{QY("&Add"), this};
  m_removeAttachmentsAction = new QAction{QY("&Remove"), this};
  ui->attachments->addAction(m_addAttachmentsAction);
  ui->attachments->addAction(m_removeAttachmentsAction);

  // Signals & slots
  connect(ui->attachments->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(onAttachmentSelectionChanged()));
  connect(m_addAttachmentsAction,            SIGNAL(activated()),                                                      this, SLOT(onAddAttachments()));
  connect(m_removeAttachmentsAction,         SIGNAL(activated()),                                                      this, SLOT(onRemoveAttachments()));

  onAttachmentSelectionChanged();
}

void
MainWindow::withSelectedAttachments(std::function<void(Attachment *)> code) {
  if (m_currentlySettingInputControlValues)
    return;

  auto selection = ui->attachments->selectionModel()->selection();
  if (selection.isEmpty())
    return;

  for (auto &indexRange : selection) {
    auto idxs = indexRange.indexes();
    if (idxs.isEmpty() || !idxs.at(0).isValid())
      continue;

    auto attachment = static_cast<Attachment *>(idxs.at(0).internalPointer());
    if (attachment) {
      code(attachment);
      m_attachmentsModel->attachmentUpdated(attachment);
    }
  }
}

void
MainWindow::onAttachmentNameEdited(QString newValue) {
  withSelectedAttachments([&](Attachment *attachment) { attachment->m_name = newValue; });
}

void
MainWindow::onAttachmentDescriptionEdited(QString newValue) {
  withSelectedAttachments([&](Attachment *attachment) { attachment->m_description = newValue; });
}

void
MainWindow::onAttachmentMIMETypeEdited(QString newValue) {
  withSelectedAttachments([&](Attachment *attachment) { attachment->m_MIMEType = newValue; });
}

void
MainWindow::onAttachmentStyleChanged(int newValue) {
  auto data = ui->attachmentStyle->itemData(newValue);
  if (!data.isValid())
    return;

  auto style = data.toInt() == Attachment::ToAllFiles ? Attachment::ToAllFiles : Attachment::ToFirstFile;
  withSelectedAttachments([&](Attachment *attachment) { attachment->m_style = style; });
}

void
MainWindow::onAddAttachments() {
  auto fileNames = selectAttachmentsToAdd();
  if (fileNames.empty())
    return;

  for (auto &fileName : fileNames) {
    m_config.m_attachments << std::make_shared<Attachment>(fileName);
    m_config.m_attachments.back()->guessMIMEType();
  }

  if (fileNames.isEmpty())
    return;

  m_attachmentsModel->setAttachments(m_config.m_attachments);
  resizeAttachmentsColumnsToContents();
}

QStringList
MainWindow::selectAttachmentsToAdd() {
  QFileDialog dlg{this};
  dlg.setNameFilter(QY("All files") + Q(" (*)"));
  dlg.setFileMode(QFileDialog::ExistingFiles);
  dlg.setDirectory(Settings::get().m_lastOpenDir);
  dlg.setWindowTitle(QY("Add attachments"));

  if (!dlg.exec())
    return QStringList{};

  Settings::get().m_lastOpenDir = dlg.directory();

  return dlg.selectedFiles();
}

void
MainWindow::onRemoveAttachments() {
  auto selection = ui->attachments->selectionModel()->selection();
  if (selection.isEmpty()) {
    enableAttachmentControls(false);
    return;
  }

  QHash<Attachment *, bool> attachmentsToDelete;

  for (auto &range : selection)
    for (auto &index : range.indexes()) {
      auto attachment = static_cast<Attachment *>(index.internalPointer());
      if (nullptr != attachment)
        attachmentsToDelete[attachment] = true;
    }

  for (int idx = m_config.m_attachments.size() - 1; 0 <= idx; --idx)
    if (attachmentsToDelete[ m_config.m_attachments[idx].get() ]) {
      m_attachmentsModel->removeRow(idx);
      m_config.m_attachments.removeAt(idx);
    }
}

void
MainWindow::resizeAttachmentsColumnsToContents()
  const {
  resizeViewColumnsToContents(ui->attachments);
}

void
MainWindow::onAttachmentSelectionChanged() {
  auto selection = ui->attachments->selectionModel()->selection();
  if (selection.isEmpty()) {
    enableAttachmentControls(false);
    return;
  }

  enableAttachmentControls(true);

  if (1 < selection.size()) {
    setAttachmentControlValues(nullptr);
    return;
  }

  auto idxs = selection.at(0).indexes();
  if (idxs.isEmpty() || !idxs.at(0).isValid())
    return;

  auto attachment = static_cast<Attachment *>(idxs.at(0).internalPointer());
  if (!attachment)
    return;

  setAttachmentControlValues(attachment);
}

void
MainWindow::enableAttachmentControls(bool enable) {
  auto controls = std::vector<QWidget *>{ui->attachmentName,     ui->attachmentNameLabel,     ui->attachmentDescription, ui->attachmentDescriptionLabel,
                                         ui->attachmentMIMEType, ui->attachmentMIMETypeLabel, ui->attachmentStyle,       ui->attachmentStyleLabel,};
  for (auto &control : controls)
    control->setEnabled(enable);

  m_removeAttachmentsAction->setEnabled(enable);
}

void
MainWindow::setAttachmentControlValues(Attachment *attachment) {
  m_currentlySettingInputControlValues = true;

  if ((nullptr == attachment) && ui->attachmentStyle->itemData(0).isValid())
    ui->attachmentStyle->insertItem(0, QY("<do not change>"));

  else if ((nullptr != attachment) && !ui->attachmentStyle->itemData(0).isValid())
    ui->attachmentStyle->removeItem(0);

  if (nullptr == attachment) {
    ui->attachmentName->setText(Q(""));
    ui->attachmentDescription->setText(Q(""));
    ui->attachmentMIMEType->setEditText(Q(""));
    ui->attachmentStyle->setCurrentIndex(0);

  } else {
    ui->attachmentName->setText(        attachment->m_name);
    ui->attachmentDescription->setText( attachment->m_description);
    ui->attachmentMIMEType->setEditText(attachment->m_MIMEType);

    Util::setComboBoxIndexIf(ui->attachmentStyle, [&](QString const &, QVariant const &data) { return data.isValid() && (data.toInt() == static_cast<int>(attachment->m_style)); });
  }

  m_currentlySettingInputControlValues = false;
}
