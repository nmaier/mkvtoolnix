/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the AAC output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_AAC_H
#define __P_AAC_H

#include "os.h"

#include "byte_buffer.h"
#include "common.h"
#include "pr_generic.h"
#include "aac_common.h"

class aac_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno, last_timecode, num_packets_same_tc;
  int64_t bytes_skipped;
  int samples_per_sec, channels, id, profile;
  bool headerless, emphasis_present;
  byte_buffer_c byte_buffer;

public:
  aac_packetizer_c(generic_reader_c *_reader, int _id, int _profile,
                   int _samples_per_sec, int _channels,
                   track_info_c &_ti, bool emphasis_present,
                   bool _headerless = false) throw (error_c);
  virtual ~aac_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "AAC";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);

private:
  virtual unsigned char *get_aac_packet(unsigned long *header,
                                        aac_header_t *aacheader);
  virtual int process_headerless(packet_cptr packet);

  virtual void dump_debug_info();
};

#endif // __P_AAC_H
