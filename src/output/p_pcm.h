/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_pcm.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the PCM output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_PCM_H
#define __P_PCM_H

#include "os.h"

#include "byte_buffer.h"
#include "common.h"
#include "pr_generic.h"

class pcm_packetizer_c: public generic_packetizer_c {
private:
  int packetno, bps, channels, bits_per_sample, packet_size;
  int64_t bytes_output, skip_bytes;
  unsigned long samples_per_sec;
  bool big_endian;
  byte_buffer_c buffer;

public:
  pcm_packetizer_c(generic_reader_c *nreader, unsigned long nsamples_per_sec,
                   int nchannels, int nbits_per_sample, track_info_c *nti,
                   bool nbig_endian = false)
    throw (error_c);
  virtual ~pcm_packetizer_c();

  virtual int process(memory_c &mem, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();
  virtual void flush();

  virtual void dump_debug_info();
};

#endif // __P_PCM_H
