#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/job_widget/mux_job.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"

MuxJob::MuxJob(Status status,
               MuxConfigPtr const &config)
  : Job{status}
  , m_config{config}
  , m_thread{}
{
}

MuxJob::~MuxJob() {
}

void
MuxJob::abort() {
  if (m_thread)
    m_thread->abort();
}

void
MuxJob::start() {
  assert(!m_thread);

  setStatus(Job::Running);

  m_thread = new MuxJobThread{this, *m_config};
  m_thread->start();
}

void
MuxJob::threadFinished() {
  m_thread = nullptr;
}

// ------------------------------------------------------------

MuxJobThread::MuxJobThread(MuxJob *job,
                           MuxConfig const &config)
  : QThread{}
  , m_aborted{false}
  , m_config(config)
{
  connect(this, SIGNAL(progressChanged(unsigned int)), job, SLOT(setProgress(unsigned int)));
  connect(this, SIGNAL(statusChanged(Job::Status)),    job, SLOT(setStatus(Job::Status)));
  connect(this, SIGNAL(finished()),                    job, SLOT(threadFinished()));
}

void
MuxJobThread::abort() {
  m_aborted = true;
}

void
MuxJobThread::run() {
  m_aborted = false;
  emit progressChanged(0);
  emit statusChanged(Job::Running);

  // TODO: MuxJobThread::run
  for (int i = 0; i < 10; i++) {
    msleep(200);
    emit progressChanged(i * 10);
  }

  emit progressChanged(100);
  emit statusChanged(Job::DoneOk);
}
