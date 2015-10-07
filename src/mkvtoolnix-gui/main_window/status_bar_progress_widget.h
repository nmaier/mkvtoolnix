#ifndef MTX_MKVTOOLNIX_GUI_MAIN_WINDOW_STATUS_BAR_PROGRESS_WIDGET_H
#define MTX_MKVTOOLNIX_GUI_MAIN_WINDOW_STATUS_BAR_PROGRESS_WIDGET_H

#include "common/common_pch.h"

#include <QWidget>

class QTreeView;

namespace Ui {
class StatusBarProgressWidget;
}

class StatusBarProgressWidget : public QWidget {
  Q_OBJECT;

protected:
  Ui::StatusBarProgressWidget *ui;

public:
  explicit StatusBarProgressWidget(QWidget *parent = nullptr);
  ~StatusBarProgressWidget();

public slots:
  void setProgress(unsigned int progress, unsigned int totalProgress);
};

#endif // MTX_MKVTOOLNIX_GUI_MAIN_WINDOW_STATUS_BAR_PROGRESS_WIDGET_H
