#include "common/common_pch.h"

#include "mmg-qt/main_window.h"
#include "mmg-qt/forms/main_window.h"

MainWindow::MainWindow(QWidget *parent)
  : QMainWindow{parent}
  , ui{new Ui::MainWindow}
{
  ui->setupUi(this);
}

MainWindow::~MainWindow() {
  delete ui;
}
