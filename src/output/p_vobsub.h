/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_vobsub.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the VobSub output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_VOBSUB_H
#define __P_VOBSUB_H

#include "os.h"

#include "common.h"
#include "compression.h"
#include "pr_generic.h"

class vobsub_packetizer_c: public generic_packetizer_c {
private:
  unsigned char *idx_data;
  int idx_data_size, aid;
  bool extract_from_mpeg, mpeg_version_warning_printed;
  int64_t packet_num, spu_size, overhead;

public:
  vobsub_packetizer_c(generic_reader_c *nreader,
                      const void *nidx_data, int nidx_data_size,
                      bool nextract_from_mpeg, track_info_c *nti)
    throw (error_c);
  virtual ~vobsub_packetizer_c();

  virtual int process(memory_c &mem,
                      int64_t old_timecode = -1, int64_t duration = -1,
                      int64_t bref = -1, int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();

protected:
  virtual int64_t extract_duration(unsigned char *data, int buf_size,
                                   int64_t timecode);
  virtual int deliver_packet(unsigned char *buf, int size, int64_t timecode,
                             int64_t default_duration);
};

#endif // __P_VOBSUB_H
