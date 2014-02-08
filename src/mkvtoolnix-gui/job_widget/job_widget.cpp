#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/job_widget/job_widget.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"
#include "mkvtoolnix-gui/forms/job_widget.h"
#include "mkvtoolnix-gui/util/util.h"

#include <QList>
#include <QMessageBox>
#include <QString>

JobWidget::JobWidget(QWidget *parent)
  : QWidget{parent}
  , ui{new Ui::JobWidget}
  , m_model{new JobModel{this}}
  , m_startAction{new QAction{this}}
  , m_addAction{new QAction{this}}
  , m_removeAction{new QAction{this}}
  , m_removeDoneAction{new QAction{this}}
  , m_removeDoneOkAction{new QAction{this}}
  , m_removeAllAction{new QAction{this}}
{
  // Setup UI controls.
  ui->setupUi(this);

  setupUiControls();
  retranslateUI();

  m_model->start();
}

JobWidget::~JobWidget() {
  delete ui;
}

JobModel *
JobWidget::getModel()
  const {
  return m_model;
}

void
JobWidget::setupUiControls() {
  ui->jobs->setModel(m_model);

  connect(m_startAction,        SIGNAL(triggered()), this, SLOT(onStart()));
  connect(m_addAction,          SIGNAL(triggered()), this, SLOT(onAdd()));
  connect(m_removeAction,       SIGNAL(triggered()), this, SLOT(onRemove()));
  connect(m_removeDoneAction,   SIGNAL(triggered()), this, SLOT(onRemoveDone()));
  connect(m_removeDoneOkAction, SIGNAL(triggered()), this, SLOT(onRemoveDoneOk()));
  connect(m_removeAllAction,    SIGNAL(triggered()), this, SLOT(onRemoveAll()));
}

void
JobWidget::onContextMenu(QPoint pos) {
  bool hasJobs      = m_model->hasJobs();
  bool hasSelection = !m_model->selectedJobs(ui->jobs).isEmpty();

  m_startAction->setEnabled(hasSelection);
  m_removeAction->setEnabled(hasSelection);
  m_removeDoneAction->setEnabled(hasJobs);
  m_removeDoneOkAction->setEnabled(hasJobs);
  m_removeAllAction->setEnabled(hasJobs);

  QMenu menu{this};

  menu.addAction(m_addAction);
  menu.addSeparator();
  menu.addAction(m_startAction);
  menu.addSeparator();
  menu.addAction(m_removeAction);
  menu.addAction(m_removeDoneAction);
  menu.addAction(m_removeDoneOkAction);
  menu.addAction(m_removeAllAction);

  menu.exec(ui->jobs->viewport()->mapToGlobal(pos));
}

void
JobWidget::onStart() {
  auto jobs = m_model->selectedJobs(ui->jobs);
  for (auto const &job : jobs)
    job->setPendingAuto();

  m_model->startNextAutoJob();
}

void
JobWidget::onAdd() {
  auto job           = std::make_shared<MuxJob>(Job::PendingManual, std::make_shared<MuxConfig>());
  job->m_description = to_qs(boost::format("Yay %1%") % job->m_id);
  job->m_dateAdded   = QDateTime::currentDateTime();

  addJob(std::static_pointer_cast<Job>(job));
}

void
JobWidget::onRemove() {
  auto idsToRemove  = QMap<uint64_t, bool>{};
  auto selectedJobs = m_model->selectedJobs(ui->jobs);

  for (auto const &job : selectedJobs)
    idsToRemove[job->m_id] = true;

  m_model->removeJobsIf([&](Job const &job) { return idsToRemove[job.m_id]; });
}

void
JobWidget::onRemoveDone() {
  m_model->removeJobsIf([this](Job const &job) {
      return (Job::DoneOk       == job.m_status)
          || (Job::DoneWarnings == job.m_status)
          || (Job::Failed       == job.m_status)
          || (Job::Aborted      == job.m_status);
    });
}

void
JobWidget::onRemoveDoneOk() {
  m_model->removeJobsIf([this](Job const &job) { return Job::DoneOk == job.m_status; });
}

void
JobWidget::onRemoveAll() {
  m_model->removeJobsIf([this](Job const &) { return true; });
}

void
JobWidget::resizeColumnsToContents()
  const {
  Util::resizeViewColumnsToContents(ui->jobs);
}

void
JobWidget::addJob(JobPtr const &job) {
  m_model->add(job);
  resizeColumnsToContents();
}

void
JobWidget::retranslateUI() {
  m_startAction->setText(QY("&Start selected jobs automatically"));
  m_addAction->setText(QY("&Add random job"));
  m_removeAction->setText(QY("&Remove selected jobs"));
  m_removeDoneAction->setText(QY("Remove &completed jobs"));
  m_removeDoneOkAction->setText(QY("Remove &successfully completed jobs"));
  m_removeAllAction->setText(QY("Remove a&ll jobs"));
}
