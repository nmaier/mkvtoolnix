/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_dts.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id$
    \brief class definition for the DTS output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_DTS_H
#define __P_DTS_H

#include "common.h"
#include "pr_generic.h"
#include "dts_common.h"

class dts_packetizer_c: public generic_packetizer_c {
private:
  int64_t samples_written, bytes_written;

  int buffer_size;

  unsigned char *packet_buffer;

  dts_header_t first_header;
  dts_header_t last_header;

public:
  bool skipping_is_normal;

  dts_packetizer_c(generic_reader_c *nreader, const dts_header_t &dts_header,
                   track_info_t *nti) throw (error_c);
  virtual ~dts_packetizer_c();

  virtual int process(unsigned char *buf, int size, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

private:
  virtual void add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_dts_packet(dts_header_t &dts_header);
  virtual int dts_packet_available();
  virtual void remove_dts_packet(int pos, int framesize);

  virtual void dump_debug_info();
};

#endif // __P_DTS_H
