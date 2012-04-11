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
{
  ui->setupUi(this);

  ui->files->setModel(m_filesModel);
  QObject::connect(ui->files, SIGNAL(expanded(QModelIndex const &)), this, SLOT(resizeFilesColumnsToContents()));

  setWindowIcon(Util::loadIcon(Q("mkvmergeGUI.png"), QList<int>{} << 32 << 48 << 64 << 128 << 256));
}

MainWindow::~MainWindow() {
  delete ui;
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
    resizeFilesColumnsToContents();
  }
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
  if (identifier.identify())
    m_config.m_files << identifier.file();
}

void
MainWindow::resizeFilesColumnsToContents()
  const {
  auto columnCount = m_filesModel->columnCount(QModelIndex{});
  for (auto column = 0; columnCount > column; ++column)
    ui->files->resizeColumnToContents(column);
}
