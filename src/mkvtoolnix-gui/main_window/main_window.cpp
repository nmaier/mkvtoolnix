#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/forms/main_window.h"
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
  , m_currentlySettingInputControlValues{false}
{
  // Setup UI controls.
  ui->setupUi(this);

  ui->files->setModel(m_filesModel);
  ui->tracks->setModel(m_tracksModel);

  ui->splitMaxFiles->setMaximum(std::numeric_limits<int>::max());

  setupComboBoxContent();
  setupControlLists();

  enableInputControls(m_allInputControls, false);

  // Setup window properties.
  setWindowIcon(Util::loadIcon(Q("mkvmergeGUI.png"), QList<int>{} << 32 << 48 << 64 << 128 << 256));

  // Connect signals & slots.
  QObject::connect(ui->files,  SIGNAL(expanded(QModelIndex const &)), this, SLOT(resizeFilesColumnsToContents()));
  QObject::connect(ui->tracks, SIGNAL(expanded(QModelIndex const &)), this, SLOT(resizeTracksColumnsToContents()));
  QObject::connect(ui->tracks->selectionModel(), SIGNAL(selectionChanged(const QItemSelection &, const QItemSelection &)), this, SLOT(onTrackSelectionChanged()));
}

MainWindow::~MainWindow() {
  delete ui;
}

void
MainWindow::setStatusBarMessage(QString const &message) {
  ui->statusBar->showMessage(message, 3000);
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
  auto fileName = QFileDialog::getSaveFileName(this, Q(""), Settings::get().m_lastConfigDir.path(), QY("MKVToolNix GUI config files") + Q(" (*.mtxcfg);;") + QY("All files") + Q(" (*)"));
  if (fileName.isEmpty())
    return;

  m_config.save(fileName);
  Settings::get().m_lastConfigDir = QFileInfo{fileName}.path();
  Settings::get().save();

  setStatusBarMessage(QY("The configuration has been saved."));
}

void
MainWindow::onOpenConfig() {
  // TODO
}

void
MainWindow::onNew() {
  // TODO
}

void
MainWindow::onAddToJobQueue() {
  // TODO
}

void
MainWindow::onStartMuxing() {
  // TODO
}

QString
MainWindow::getOpenFileName(QString const &title,
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
MainWindow::getSaveFileName(QString const &title,
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
