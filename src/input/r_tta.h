/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_avi.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file r_tta.h
    \version $Id$
    \brief class definitions for the TTA demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_TTA_H
#define __R_TTA_H

#include "os.h"

#include <stdio.h>

#include "common.h"
#include "error.h"
#include "mm_io.h"
#include "pr_generic.h"

class tta_reader_c: public generic_reader_c {
private:
  unsigned char *chunk;
  mm_io_c *mm_io;
  int64_t bytes_processed, size;
  uint16_t channels, bits_per_sample;
  uint32_t sample_rate, data_length;
  vector<uint32_t> seek_points;
  int pos;

public:
  tta_reader_c(track_info_c *nti) throw (error_c);
  virtual ~tta_reader_c();

  virtual int read(generic_packetizer_c *ptzr, bool force = false);
  virtual int display_priority();
  virtual void display_progress(bool final = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif // __R_TTA_H
