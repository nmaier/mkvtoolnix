#include "common/common_pch.h"

#include "common/qt.h"
#include "mkvtoolnix-gui/job_widget/job.h"

uint64_t Job::ms_next_id = 0;

Job::Job(Status status)
  : m_id{ms_next_id++}
  , m_status{status}
  , m_progress{}
  , m_exitCode{std::numeric_limits<unsigned int>::max()}
{
}

Job::~Job() {
}

void
Job::action(std::function<void()> code) {
  m_mutex.lock();
  code();
  m_mutex.unlock();
}

void
Job::setStatus(Status status) {
  action([this,status]() {
      if (status != m_status) {
        m_status = status;

        if (Running == status)
          m_dateStarted = QDateTime::currentDateTime();

        else if ((DoneOk == status) || (DoneWarnings == status) || (Failed == status) || (Aborted == status))
          m_dateFinished = QDateTime::currentDateTime();

        emit statusChanged(m_id, m_status);
      }
    });
}

void
Job::setProgress(unsigned int progress) {
  action([this,progress]() {
      if (progress != m_progress) {
        m_progress = progress;
        emit progressChanged(m_id, m_progress);
      }
    });
}

// ------------------------------------------------------------

MuxJob::MuxJob(Status status)
  : Job{status}
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
  m_thread = new MuxJobThread{this};
  m_thread->start();
}

void
MuxJob::threadFinished() {
  m_thread = nullptr;
}

// ------------------------------------------------------------

MuxJobThread::MuxJobThread(MuxJob *job)
  : QThread{}
  , m_aborted{false}
{
  connect(this, SIGNAL(progressChanged(unsigned int)), job, SLOT(setProgress(unsigned int)));
  connect(this, SIGNAL(statusChanged(unsigned int)),   job, SLOT(setStatus(unsigned int)));
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
    msleep(500);
    emit progressChanged(i * 10);
  }

  emit progressChanged(100);
  emit statusChanged(Job::DoneOk);
}
