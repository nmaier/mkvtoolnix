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
  static int const RowNotFound = -1;

public:
  JobModel(QObject *parent);
  virtual ~JobModel();

  QList<Job *> selectedJobs(QAbstractItemView *view) const;
  Job *fromId(uint64_t id) const;
  Job *fromIndex(QModelIndex const &index) const;
  int rowFromId(uint64_t id) const;
  bool hasJobs() const;

  void removeJobsIf(std::function<bool(Job const &)> predicate);
  void add(JobPtr const &job);

  void start();
  void stop();

public slots:
  void onStatusChanged(uint64_t id, Job::Status status);
  void onProgressChanged(uint64_t id, unsigned int progress);

protected:
  QList<QStandardItem *> createRow(Job const &job) const;

public:                         // static
  static QString displayableJobType(Job const &job);
  static QString displayableJobStatus(Job const &job);
  static QString displayableDate(QDateTime const &date);
};

#endif  // MTX_MKVTOOLNIXGUI_JOB_MODEL_H
