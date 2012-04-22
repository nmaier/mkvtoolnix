#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/forms/main_window.h"
#include "mkvtoolnix-gui/main_window/main_window.h"
#include "mkvtoolnix-gui/util/settings.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QIcon>

MainWindow *MainWindow::ms_mainWindow = nullptr;

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow{parent}
  , ui{new Ui::MainWindow}
  , mergeButton{nullptr}
  , infoButton{nullptr}
  , extractButton{nullptr}
  , propeditButton{nullptr}
{
  ms_mainWindow = this;

  // Setup UI controls.
  ui->setupUi(this);

  setupToolSelector();

  // Setup window properties.
  setWindowIcon(Util::loadIcon(Q("mkvmergeGUI.png"), QList<int>{} << 32 << 48 << 64 << 128 << 256));
}

MainWindow::~MainWindow() {
  delete ui;
}

void
MainWindow::setStatusBarMessage(QString const &message) {
  ui->statusBar->showMessage(message, 3000);
}

Ui::MainWindow *
MainWindow::getUi() {
  return ui;
}

void
MainWindow::setupToolSelector() {
  ui->toolSelector->setIconSize(QSize{48, 48});

  mergeButton = ui->toolSelector->addAction(Util::loadIcon(Q("mkvmerge.png"), QList<int>{} << 32 << 48 << 64 << 128 << 256), QY("merge"));
  mergeButton->setCheckable(true);
}

MainWindow *
MainWindow::get() {
  return ms_mainWindow;
}
