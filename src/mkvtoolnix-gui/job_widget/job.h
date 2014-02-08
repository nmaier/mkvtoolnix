#ifndef MTX_MKVTOOLNIX_GUI_JOB_H
#define MTX_MKVTOOLNIX_GUI_JOB_H

#include "common/common_pch.h"

#include <QDateTime>
#include <QObject>
#include <QMutex>
#include <QString>
#include <QStringList>
#include <QThread>

class Job: public QObject {
  Q_OBJECT;
  Q_ENUMS(Status);

public:
  enum Status {
    PendingManual,
    PendingAuto,
    Running,
    DoneOk,
    DoneWarnings,
    Failed,
    Aborted,
    Disabled,
  };

private:
  static uint64_t ms_next_id;

public:
  uint64_t m_id;
  Status m_status;
  QString m_description;
  QStringList m_output, m_warnings, m_errors;
  unsigned int m_progress, m_exitCode;
  QDateTime m_dateAdded, m_dateStarted, m_dateFinished;

  QMutex m_mutex;

public:
  Job(Status status = PendingManual);
  virtual ~Job();

  void action(std::function<void()> code);
  bool isToBeProcessed() const;

  virtual void abort() = 0;
  virtual void start() = 0;

  void setPendingAuto();

public slots:
  void setStatus(Job::Status status);
  void setProgress(unsigned int progress);

signals:
  void statusChanged(uint64_t id, Job::Status status);
  void progressChanged(uint64_t id, unsigned int progress);
};
typedef std::shared_ptr<Job> JobPtr;

class MuxJob;
class MuxConfig;
typedef std::shared_ptr<MuxConfig> MuxConfigPtr;

class MuxJobThread: public QThread {
  Q_OBJECT;
protected:
  volatile bool m_aborted;
  MuxConfig const &m_config;

public:
  MuxJobThread(MuxJob *job, MuxConfig const &config);

  void abort();

protected:
  virtual void run();

signals:
  void progressChanged(unsigned int progress);
  void statusChanged(Job::Status status);
};

class MuxJob: public Job {
  Q_OBJECT;
protected:
  MuxConfigPtr m_config;
  MuxJobThread *m_thread;

public:
  MuxJob(Status status, MuxConfigPtr const &config);
  virtual ~MuxJob();

  virtual void abort();
  virtual void start();

public slots:
  void threadFinished();
};

#endif  // MTX_MKVTOOLNIX_GUI_JOB_H
