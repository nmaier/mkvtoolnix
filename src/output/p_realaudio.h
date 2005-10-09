/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the RealAudio output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_REALAUDIO_H
#define __P_REALAUDIO_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"
#include "smart_pointers.h"

class ra_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno;
  int samples_per_sec, channels, bits_per_sample;
  uint32_t fourcc;
  memory_cptr private_data;
  bool skip_to_keyframe, buffer_until_keyframe;
  vector<memory_c *> buffered_packets;
  vector<int64_t> buffered_timecodes, buffered_durations;

public:
  ra_packetizer_c(generic_reader_c *_reader, int _samples_per_sec,
                  int _channels, int _bits_per_sample, uint32_t _fourcc,
                  unsigned char *_private_data, int _private_size,
                  track_info_c &_ti)
    throw (error_c);
  virtual ~ra_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "RealAudio";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);

protected:
  virtual void dump_debug_info();
};

#endif // __P_REALAUDIO_H
