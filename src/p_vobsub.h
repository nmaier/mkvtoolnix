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
  int idx_data_size;
  bool compressed;
  int compression_type, compressed_type;
  int64_t raw_size, compressed_size, items;
  compression_c *compressor, *decompressor;

public:
  vobsub_packetizer_c(generic_reader_c *nreader,
                      const void *nidx_data, int nidx_data_size,
                      int ncompression_type, int ncompressed_type,
                      track_info_t *nti) throw (error_c);
  virtual ~vobsub_packetizer_c();

  virtual int process(unsigned char *buf, int size, int64_t old_timecode = -1,
                      int64_t duration = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();
};

#endif // __P_VOBSUB_H
