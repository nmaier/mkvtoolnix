/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * class definition for the AAC output module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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
  unsigned long samples_per_sec;
  int channels, id, profile;
  bool headerless, emphasis_present;
  byte_buffer_c byte_buffer;

public:
  aac_packetizer_c(generic_reader_c *nreader, int nid, int nprofile,
                   unsigned long nsamples_per_sec, int nchannels,
                   track_info_c *nti, bool emphasis_present,
                   bool nheaderless = false) throw (error_c);
  virtual ~aac_packetizer_c();

  virtual int process(memory_c &mem, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "AAC";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src);

private:
  virtual unsigned char *get_aac_packet(unsigned long *header,
                                        aac_header_t *aacheader);

  virtual void dump_debug_info();
};

#endif // __P_AAC_H
