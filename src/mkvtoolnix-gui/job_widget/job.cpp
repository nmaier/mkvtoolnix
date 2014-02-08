#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/job_widget/job.h"
#include "mkvtoolnix-gui/merge_widget/mux_config.h"

uint64_t Job::ms_next_id = 0;

Job::Job(Status status)
  : m_id{ms_next_id++}
  , m_status{status}
  , m_progress{}
  , m_exitCode{std::numeric_limits<unsigned int>::max()}
  , m_mutex{QMutex::Recursive}
{
}

Job::~Job() {
}

void
Job::setStatus(Status status) {
  QMutexLocker locked{&m_mutex};

  if (status == m_status)
    return;

  m_status = status;

  if (Running == status)
    m_dateStarted = QDateTime::currentDateTime();

  else if ((DoneOk == status) || (DoneWarnings == status) || (Failed == status) || (Aborted == status))
    m_dateFinished = QDateTime::currentDateTime();

  emit statusChanged(m_id, m_status);
}

bool
Job::isToBeProcessed()
  const {
  return (Running == m_status) || (PendingAuto == m_status);
}

void
Job::setProgress(unsigned int progress) {
  QMutexLocker locked{&m_mutex};

  if (progress == m_progress)
    return;

  m_progress = progress;
  emit progressChanged(m_id, m_progress);
}

void
Job::setPendingAuto() {
  QMutexLocker locked{&m_mutex};

  if ((PendingAuto != m_status) && (Running != m_status))
    setStatus(PendingAuto);
}

// ------------------------------------------------------------

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
