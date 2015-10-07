#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/job_widget/job_widget.h"
#include "mkvtoolnix-gui/job_widget/mux_job.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/merge_widget/merge_widget.h"
#include "mkvtoolnix-gui/forms/main_window.h"
#include "mkvtoolnix-gui/forms/merge_widget.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>
#include <QInputDialog>
#include <QList>
#include <QMessageBox>
#include <QString>
#include <QTimer>

MergeWidget::MergeWidget(QWidget *parent)
  : QWidget{parent}
  , ui{new Ui::MergeWidget}
  , m_filesModel{new SourceFileModel{this}}
  , m_tracksModel{new TrackModel{this}}
  , m_currentlySettingInputControlValues{false}
  , m_addFilesAction{new QAction{this}}
  , m_appendFilesAction{new QAction{this}}
  , m_addAdditionalPartsAction{new QAction{this}}
  , m_removeFilesAction{new QAction{this}}
  , m_removeAllFilesAction{new QAction{this}}
  , m_attachmentsModel{new AttachmentModel{this, m_config.m_attachments}}
  , m_addAttachmentsAction{new QAction{this}}
  , m_removeAttachmentsAction{new QAction{this}}
{
  // Setup UI controls.
  ui->setupUi(this);

  m_filesModel->setTracksModel(m_tracksModel);

  setupMenu();
  setupInputControls();
  setupOutputControls();
  setupAttachmentsControls();

  retranslateUI();

  setControlValuesFromConfig();

  auto timer = new QTimer(this);
  connect(timer, SIGNAL(timeout()), this, SLOT(onShowMkvmergeOptions()));
  timer->start(1000);
}

MergeWidget::~MergeWidget() {
  delete ui;
}

void
MergeWidget::onShowMkvmergeOptions() {
  auto options = m_config.buildMkvmergeOptions();
  ui->debugOutput->setPlainText(Q("num: %1\n").arg(options.size()) + options.join(Q("\n")));
  ui->cmdUnix->setPlainText(Util::escape(options, Util::EscapeShellUnix).join(Q(" ")));
  ui->cmdWindows->setPlainText(Util::escape(options, Util::EscapeShellWindows).join(Q(" ")));
}

void
MergeWidget::onSaveConfig() {
  if (m_config.m_configFileName.isEmpty())
    onSaveConfigAs();
  else {
    m_config.save();
    MainWindow::get()->setStatusBarMessage(QY("The configuration has been saved."));
  }
}

void
MergeWidget::onSaveConfigAs() {
  auto fileName = QFileDialog::getSaveFileName(this, Q(""), Settings::get().m_lastConfigDir.path(), QY("MKVToolNix GUI config files") + Q(" (*.mtxcfg);;") + QY("All files") + Q(" (*)"));
  if (fileName.isEmpty())
    return;

  m_config.save(fileName);
  Settings::get().m_lastConfigDir = QFileInfo{fileName}.path();
  Settings::get().save();

  MainWindow::get()->setStatusBarMessage(QY("The configuration has been saved."));
}

void
MergeWidget::onOpenConfig() {
  auto fileName = QFileDialog::getOpenFileName(this, Q(""), Settings::get().m_lastConfigDir.path(), QY("MKVToolNix GUI config files") + Q(" (*.mtxcfg);;") + QY("All files") + Q(" (*)"));
  if (fileName.isEmpty())
    return;

  Settings::get().m_lastConfigDir = QFileInfo{fileName}.path();
  Settings::get().save();

  try {
    m_config.load(fileName);
    MainWindow::get()->setStatusBarMessage(QY("The configuration has been loaded."));

  } catch (mtx::InvalidSettingsX &) {
    m_config.reset();
    QMessageBox::critical(this, QY("Error loading settings file"), QY("The settings file '%1' contains invalid settings and was not loaded.").arg(fileName));
  }

  setControlValuesFromConfig();
}

void
MergeWidget::onNew() {
  m_config.reset();
  setControlValuesFromConfig();
  MainWindow::get()->setStatusBarMessage(QY("The configuration has been reset."));
}

void
MergeWidget::onAddToJobQueue() {
  addToJobQueue(false);
}

void
MergeWidget::onStartMuxing() {
  addToJobQueue(true);
}

QString
MergeWidget::getOpenFileName(QString const &title,
                             QString const &filter,
                             QLineEdit *lineEdit) {
  auto fullFilter = filter;
  if (!fullFilter.isEmpty())
    fullFilter += Q(";;");
  fullFilter += QY("All files") + Q(" (*)");

  auto dir      = lineEdit->text().isEmpty() ? Settings::get().m_lastOpenDir.path() : QFileInfo{ lineEdit->text() }.path();
  auto fileName = QFileDialog::getOpenFileName(this, title, dir, fullFilter);
  if (fileName.isEmpty())
    return fileName;

  Settings::get().m_lastOpenDir = QFileInfo{fileName}.path();
  Settings::get().save();

  lineEdit->setText(fileName);

  return fileName;
}

QString
MergeWidget::getSaveFileName(QString const &title,
                             QString const &filter,
                             QLineEdit *lineEdit) {
  auto fullFilter = filter;
  if (!fullFilter.isEmpty())
    fullFilter += Q(";;");
  fullFilter += QY("All files") + Q(" (*)");

  auto dir      = lineEdit->text().isEmpty() ? Settings::get().m_lastOutputDir.path() : QFileInfo{ lineEdit->text() }.path();
  auto fileName = QFileDialog::getSaveFileName(this, title, dir, fullFilter);
  if (fileName.isEmpty())
    return fileName;

  Settings::get().m_lastOutputDir = QFileInfo{fileName}.path();
  Settings::get().save();

  lineEdit->setText(fileName);

  return fileName;
}

void
MergeWidget::setupMenu() {
  auto mwUi = MainWindow::get()->getUi();

  connect(mwUi->actionNew,                        SIGNAL(triggered()), this,              SLOT(onNew()));
  connect(mwUi->actionOpen,                       SIGNAL(triggered()), this,              SLOT(onOpenConfig()));
  connect(mwUi->actionSave,                       SIGNAL(triggered()), this,              SLOT(onSaveConfig()));
  connect(mwUi->actionSaveAs,                     SIGNAL(triggered()), this,              SLOT(onSaveConfigAs()));
  // connect(mwUi->actionSaveOptionFile,             SIGNAL(triggered()), this,              SLOT(onSaveOpenFile()));
  connect(mwUi->actionExit,                       SIGNAL(triggered()), MainWindow::get(), SLOT(close()));

  connect(mwUi->actionStartMuxing,                SIGNAL(triggered()), this,              SLOT(onStartMuxing()));
  connect(mwUi->actionAddToJobQueue,              SIGNAL(triggered()), this,              SLOT(onAddToJobQueue()));
  // connect(mwUi->actionShowMkvmergeCommandLine,    SIGNAL(triggered()), this,              SLOT(onShowCommandLine()));
  // connect(mwUi->actionCopyCommandLineToClipboard, SIGNAL(triggered()), this,              SLOT(onCopyCommandLineToClipboard()));
}

void
MergeWidget::setControlValuesFromConfig() {
  m_filesModel->setSourceFiles(m_config.m_files);
  m_tracksModel->setTracks(m_config.m_tracks);
  m_attachmentsModel->setAttachments(m_config.m_attachments);

  resizeFilesColumnsToContents();
  resizeTracksColumnsToContents();
  resizeAttachmentsColumnsToContents();

  onFileSelectionChanged();
  onTrackSelectionChanged();
  setOutputControlValues();
  onAttachmentSelectionChanged();
}

void
MergeWidget::retranslateUI() {
  retranslateInputUI();
  retranslateAttachmentsUI();
}

void
MergeWidget::addToJobQueue(bool startNow) {
  auto newConfig     = std::make_shared<MuxConfig>(m_config);
  auto job           = std::make_shared<MuxJob>(startNow ? Job::PendingAuto : Job::PendingManual, newConfig);
  job->m_dateAdded   = QDateTime::currentDateTime();
  job->m_description = QY("Merge to %1").arg(newConfig->m_destination);

  if (!startNow) {
    auto newDescription = QString{};

    while (newDescription.isEmpty()) {
      bool ok = false;
      newDescription = QInputDialog::getText(this, QY("Enter job description"), QY("Please enter the new job's description."), QLineEdit::Normal, job->m_description, &ok);
      if (!ok)
        return;
    }

    job->m_description = newDescription;
  }

  MainWindow::getJobWidget()->addJob(std::static_pointer_cast<Job>(job));
}
