/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_realaudio.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the RealAudio output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_REALAUDIO_H
#define __P_REALAUDIO_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"

class ra_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno;
  unsigned long samples_per_sec;
  int channels, bits_per_sample;
  uint32_t fourcc;
  unsigned char *private_data;
  int private_size;
  bool skip_to_keyframe, buffer_until_keyframe;
  vector<memory_c *> buffered_packets;
  vector<int64_t> buffered_timecodes, buffered_durations;

public:
  ra_packetizer_c(generic_reader_c *nreader, unsigned long nsamples_per_sec,
                  int nchannels, int nbits_per_sample, uint32_t nfourcc,
                  unsigned char *nprivate_data, int nprivate_size,
                  track_info_c *nti)
    throw (error_c);
  virtual ~ra_packetizer_c();

  virtual int process(memory_c &mem, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "RealAudio";
  }
  virtual int can_connect_to(generic_packetizer_c *src);

protected:
  virtual void dump_debug_info();
};

#endif // __P_REALAUDIO_H
