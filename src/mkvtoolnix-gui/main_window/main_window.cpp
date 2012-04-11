#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/forms/main_window.h"
#include "mkvtoolnix-gui/util/file_identifier.h"
#include "mkvtoolnix-gui/util/file_type_filter.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QFileDialog>
#include <QIcon>
#include <QList>
#include <QString>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow{parent}
  , ui{new Ui::MainWindow}
  , m_filesModel{new SourceFileModel{this}}
  , m_tracksModel{new TrackModel{this}}
{
  ui->setupUi(this);

  ui->files->setModel(m_filesModel);
  ui->tracks->setModel(m_tracksModel);

  QObject::connect(ui->files,  SIGNAL(expanded(QModelIndex const &)), this, SLOT(resizeFilesColumnsToContents()));
  QObject::connect(ui->tracks, SIGNAL(expanded(QModelIndex const &)), this, SLOT(resizeTracksColumnsToContents()));

  setWindowIcon(Util::loadIcon(Q("mkvmergeGUI.png"), QList<int>{} << 32 << 48 << 64 << 128 << 256));
}

MainWindow::~MainWindow() {
  delete ui;
}

void
MainWindow::setStatusBarMessage(QString const &message) {
  ui->statusBar->showMessage(message, 3000);
}

void
MainWindow::onAddFiles() {
  auto fileNames = selectFilesToAdd();
  if (fileNames.empty())
    return;

  auto numFilesBefore = m_config.m_files.size();

  for (auto &fileName : fileNames)
    addFile(fileName, false);

  if (numFilesBefore != m_config.m_files.size()) {
    m_filesModel->setSourceFiles(m_config.m_files);
    m_tracksModel->setTracks(m_config.m_tracks);
    resizeFilesColumnsToContents();
    resizeTracksColumnsToContents();
  }
}

void
MainWindow::onSaveConfig() {
  if (m_config.m_configFileName.isEmpty())
    onSaveConfigAs();
  else {
    m_config.save();
    setStatusBarMessage(QY("The configuration has been saved."));
  }
}

void
MainWindow::onSaveConfigAs() {
  auto fileName = QFileDialog::getSaveFileName(this, QY("Select config file name"), Settings::get().m_lastConfigDir.path(), QY("MKVToolNix GUI config files (*.mtxcfg);;All files (*)"));
  if (fileName.isEmpty())
    return;

  m_config.save(fileName);
  Settings::get().m_lastConfigDir = QFileInfo(fileName).filePath();

  setStatusBarMessage(QY("The configuration has been saved."));
}

void
MainWindow::onOpenConfig() {
}

void
MainWindow::onNew() {
}

void
MainWindow::onAddToJobQueue() {
}

void
MainWindow::onStartMuxing() {
}

QStringList
MainWindow::selectFilesToAdd() {
  QFileDialog dlg{this};
  dlg.setNameFilters(FileTypeFilter::get());
  dlg.setFileMode(QFileDialog::ExistingFiles);
  dlg.setDirectory(Settings::get().m_lastOpenDir);
  dlg.setWindowTitle(QY("Add media files"));

  if (!dlg.exec())
    return QStringList{};

  Settings::get().m_lastOpenDir = dlg.directory();

  return dlg.selectedFiles();
}

void
MainWindow::addFile(QString const &fileName,
                    bool /*append*/) {
  FileIdentifier identifier{ this, fileName };
  if (!identifier.identify())
    return;

  m_config.m_files << identifier.file();
  for (auto &track : identifier.file()->m_tracks)
    m_config.m_tracks << track.get();
}

void
MainWindow::resizeFilesColumnsToContents()
  const {
  auto columnCount = m_filesModel->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    ui->files->resizeColumnToContents(column);
}

void
MainWindow::resizeTracksColumnsToContents()
  const {
  auto columnCount = m_tracksModel->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    ui->tracks->resizeColumnToContents(column);
}
