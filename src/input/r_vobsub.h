/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_vobsub.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definitions for the VobSub stream reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_VOBSUB_H
#define __R_VOBSUB_H

#include "os.h"

#include <stdio.h>

#include "common.h"
#include "mm_io.h"
#include "pr_generic.h"
#include "p_vobsub.h"

class vobsub_track_c {
public:
  char *language;
  int ptzr;
  vector<int64_t> positions, sizes, timecodes, durations;
  int idx;
  bool headers_set;

public:
  vobsub_track_c(const char *new_language) {
    language = safestrdup(new_language);
    ptzr = -1;
    idx = 0;
    headers_set = false;
  };
  ~vobsub_track_c() {
    safefree(language);
  }
};

class vobsub_reader_c: public generic_reader_c {
private:
  mm_io_c *sub_file;
  mm_text_io_c *idx_file;
  int version, ifo_data_size;
  string idx_data;

  vector<vobsub_track_c *> tracks;

public:
  vobsub_reader_c(track_info_c *nti) throw (error_c);
  virtual ~vobsub_reader_c();

  virtual int read(generic_packetizer_c *ptzr);
  virtual void set_headers();
  virtual void identify();

  static int probe_file(mm_io_c *mm_io, int64_t size);

protected:
  virtual void parse_headers();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void flush_packetizers();
};

#endif  // __R_VOBSUB_H
