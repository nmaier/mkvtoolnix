#include "common/common_pch.h"

#include "common/qt.h"
#include "mmg-qt/main_window.h"
#include "mmg-qt/forms/main_window.h"
#include "mmg-qt/util/file_identifier.h"
#include "mmg-qt/util/file_type_filter.h"
#include "mmg-qt/util/settings.h"

#include <QFileDialog>

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow{parent}
  , ui{new Ui::MainWindow}
{
  ui->setupUi(this);
}

MainWindow::~MainWindow() {
  delete ui;
}

void
MainWindow::onAddFiles() {
  auto fileNames = selectFilesToAdd();
  if (fileNames.empty())
    return;

  for (auto &fileName : fileNames)
    addFile(fileName, false);
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
  mxinfo(boost::format("res; %1%\n") % identifier.identify());
}
