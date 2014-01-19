#include "common/common_pch.h"

#include <QAbstractItemView>

#include "common/qt.h"
#include "mkvtoolnix-gui/job_model.h"
#include "mkvtoolnix-gui/util/util.h"

JobModel::JobModel(QObject *parent)
  : QStandardItemModel{parent}
  , m_started{}
{
  auto labels = QStringList{};
  labels << QY("Description") << QY("Type") << QY("Status") << QY("Date added") << QY("Date started") << QY("Date finished");
  setHorizontalHeaderLabels(labels);
  horizontalHeaderItem(3)->setTextAlignment(Qt::AlignRight);
  horizontalHeaderItem(4)->setTextAlignment(Qt::AlignRight);
}

JobModel::~JobModel() {
}

QList<Job *>
JobModel::selectedJobs(QAbstractItemView *view) {
  auto selectedIds = QMap<uint64_t, bool>{};
  Util::withSelectedIndexes(view, [&](QModelIndex const &idx) {
      if (!idx.column())
        selectedIds[ data(idx, Util::JobIdRole).value<uint64_t>() ] = true;
    });

  QList<Job *> jobs;
  for (auto const &job : m_jobs)
    if (selectedIds[job->m_id])
      jobs << job.get();

  mxinfo(boost::format("selected: %1%\n") % jobs.size());

  return jobs;
}

Job *
JobModel::getJobById(uint64_t id) {
  auto jobItr = brng::find_if(m_jobs, [id](JobPtr const &job) { return job->m_id == id; });
  return jobItr != m_jobs.end() ? jobItr->get() : nullptr;
}

Job *
JobModel::fromIndex(QModelIndex const &index) {
  return index.isValid() ? getJobById(index.data(Util::JobIdRole).value<uint64_t>()) : nullptr;
}

bool
JobModel::hasJobs()
  const {
  return !m_jobs.isEmpty();
}

QString
JobModel::displayableJobType(Job const &job) {
  return dynamic_cast<MuxJob const *>(&job) ? QY("merge")
       :                                      QY("unknown");
}

QString
JobModel::displayableJobStatus(Job const &job) {
  return job.m_status == Job::PendingManual ? QY("pending manual start")
       : job.m_status == Job::PendingAuto   ? QY("pending automatic start")
       : job.m_status == Job::Running       ? QY("running")
       : job.m_status == Job::DoneOk        ? QY("completed OK")
       : job.m_status == Job::DoneWarnings  ? QY("completed with warnings")
       : job.m_status == Job::Failed        ? QY("failed")
       : job.m_status == Job::Aborted       ? QY("aborted by user")
       : job.m_status == Job::Disabled      ? QY("disabled")
       :                                      QY("unknown");
}

QString
JobModel::displayableDate(QDateTime const &date) {
  return date.isValid() ? date.toString(Qt::ISODate) : QString{""};
}

QList<QStandardItem *>
JobModel::createRow(Job const &job)
  const {
  auto items = QList<QStandardItem *>{};

  items << (new QStandardItem{job.m_description})                << (new QStandardItem{displayableJobType(job)})            << (new QStandardItem{displayableJobStatus(job)})
        << (new QStandardItem{displayableDate(job.m_dateAdded)}) << (new QStandardItem{displayableDate(job.m_dateStarted)}) << (new QStandardItem{displayableDate(job.m_dateFinished)});

  items[0]->setData(QVariant::fromValue(job.m_id), Util::JobIdRole);
  items[3]->setTextAlignment(Qt::AlignRight);
  items[4]->setTextAlignment(Qt::AlignRight);

  return items;
}

void
JobModel::removeJobsIf(std::function<bool(Job const &)> predicate) {
  m_mutex.lock();

  for (int idx = m_jobs.size(); 0 < idx; --idx)
    if (predicate(*m_jobs[idx - 1]))
      removeRow(idx - 1);

  brng::remove_erase_if(m_jobs, [predicate](JobPtr const &job) { return predicate(*job); });

  m_mutex.unlock();
}

void
JobModel::add(JobPtr const &job) {
  m_mutex.lock();
  invisibleRootItem()->appendRow(createRow(*job));
  m_jobs << job;
  m_mutex.unlock();
}
