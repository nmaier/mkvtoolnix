#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/forms/status_bar_progress_widget.h"
#include "mkvtoolnix-gui/main_window/status_bar_progress_widget.h"

StatusBarProgressWidget::StatusBarProgressWidget(QWidget *parent)
  : QWidget{parent}
  , ui{new Ui::StatusBarProgressWidget}
{
  ui->setupUi(this);
}

StatusBarProgressWidget::~StatusBarProgressWidget() {
  delete ui;
}

void
StatusBarProgressWidget::setProgress(unsigned int progress,
                                     unsigned int totalProgress) {
  ui->progress->setValue(progress);
  ui->totalProgress->setValue(totalProgress);
}
