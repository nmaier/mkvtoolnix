#ifndef MTX_MKVTOOLNIXGUI_JOB_MODEL_H
#define MTX_MKVTOOLNIXGUI_JOB_MODEL_H

#include "common/common_pch.h"

#include "mkvtoolnix-gui/job.h"

#include <QStandardItemModel>
#include <QList>
#include <QMutex>

class QAbstractItemView;

class JobModel: public QStandardItemModel {
  Q_OBJECT;
protected:
  QList<JobPtr> m_jobs;
  QMutex m_mutex;

  bool m_started;

public:
  JobModel(QObject *parent);
  virtual ~JobModel();

  QList<Job *> selectedJobs(QAbstractItemView *view);
  Job *getJobById(uint64_t id);
  Job *fromIndex(QModelIndex const &index);
  bool hasJobs() const;

  void removeJobsIf(std::function<bool(Job const &)> predicate);
  void add(JobPtr const &job);

  void start();
  void stop();

protected:
  QList<QStandardItem *> createRow(Job const &job) const;

public:                         // static
  static QString displayableJobType(Job const &job);
  static QString displayableJobStatus(Job const &job);
  static QString displayableDate(QDateTime const &date);
};

#endif  // MTX_MKVTOOLNIXGUI_JOB_MODEL_H
