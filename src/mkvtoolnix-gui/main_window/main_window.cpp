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
{
  ui->setupUi(this);

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
