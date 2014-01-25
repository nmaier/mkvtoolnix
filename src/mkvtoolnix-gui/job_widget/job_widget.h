#ifndef MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_JOBS_WIDGET_H
#define MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_JOBS_WIDGET_H

#include "common/common_pch.h"

#include <QList>
#include <QMenu>
#include <QWidget>

#include "mkvtoolnix-gui/job_widget/job_model.h"

namespace Ui {
class JobWidget;
}

class JobWidget : public QWidget {
  Q_OBJECT;

protected:
  // UI stuff:
  Ui::JobWidget *ui;

  JobModel *m_model;

  QAction *m_startAction, *m_addAction, *m_removeAction, *m_removeDoneAction, *m_removeDoneOkAction, *m_removeAllAction;

public:
  explicit JobWidget(QWidget *parent = nullptr);
  ~JobWidget();

  JobModel *getModel() const;

public slots:
  void onStart();
  void onAdd();
  void onRemove();
  void onRemoveDone();
  void onRemoveDoneOk();
  void onRemoveAll();

  void onContextMenu(QPoint pos);

  void resizeColumnsToContents() const;

protected:
  void setupUiControls();
  void retranslateUI();
};

#endif // MTX_MKVTOOLNIX_GUI_MERGE_WIDGET_JOB_WIDGET_H
