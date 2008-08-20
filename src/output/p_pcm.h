/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the PCM output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_PCM_H
#define __P_PCM_H

#include "os.h"

#include "byte_buffer.h"
#include "common.h"
#include "pr_generic.h"

class pcm_packetizer_c: public generic_packetizer_c {
private:
  int packetno, bps, samples_per_sec, channels, bits_per_sample, packet_size;
  int64_t bytes_output;
  bool big_endian;
  byte_buffer_c buffer;

public:
  pcm_packetizer_c(generic_reader_c *_reader, int _samples_per_sec,
                   int _channels, int _bits_per_sample, track_info_c &_ti,
                   bool _big_endian = false)
    throw (error_c);
  virtual ~pcm_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();
  virtual void flush();

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "PCM";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message);
};

#endif // __P_PCM_H
