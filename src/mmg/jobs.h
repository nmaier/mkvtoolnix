/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  jobs.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <flo.wagner@gmx.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief declaration for the job queue
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __JOBS_H
#define __JOBS_H

enum job_status_t {
  jobs_pending,
  jobs_done,
  jobs_aborted,
  jobs_failed
};

typedef struct {
  int32_t id;
  job_status_t status;
  int32_t added_at, started_at, finished_at;
  wxString *description;
} job_t;

extern vector<job_t> jobs;

#endif // __JOBS_H
