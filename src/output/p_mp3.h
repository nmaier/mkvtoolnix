/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the MP3 output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_MP3_H
#define __P_MP3_H

#include "os.h"

#include "byte_buffer.h"
#include "common.h"
#include "pr_generic.h"
#include "mp3_common.h"

class mp3_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno;
  unsigned long samples_per_sec;
  int channels, spf;
  byte_buffer_c byte_buffer;
  bool codec_id_set, valid_headers_found;

public:
  mp3_packetizer_c(generic_reader_c *nreader, unsigned long nsamples_per_sec,
                   int nchannels, bool source_is_good, track_info_c *nti)
    throw (error_c);
  virtual ~mp3_packetizer_c();

  virtual int process(memory_c &mem, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "MP3";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src);

private:
  virtual unsigned char *get_mp3_packet(mp3_header_t *mp3header);

  virtual void dump_debug_info();
  virtual void handle_garbage(int64_t bytes);
};

#endif // __P_MP3_H
