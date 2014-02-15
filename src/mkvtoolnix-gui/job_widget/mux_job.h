#ifndef MTX_MKVTOOLNIX_GUI_MUX_JOB_H
#define MTX_MKVTOOLNIX_GUI_MUX_JOB_H

#include "common/common_pch.h"

#include <QThread>

#include "mkvtoolnix-gui/job_widget/job.h"

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

#endif // MTX_MKVTOOLNIX_GUI_MUX_JOB_H
