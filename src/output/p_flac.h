/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_flac.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the Flac packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "config.h"

#ifndef __P_FLAC_H
#define __P_FLAC_H

#include "os.h"

#if defined(HAVE_FLAC_FORMAT_H)

#include "common.h"
#include "pr_generic.h"

class flac_packetizer_c: public generic_packetizer_c {
private:
  unsigned char *header;
  int l_header, sample_rate, channels, bits_per_sample;
  int64_t last_timecode;

public:
  flac_packetizer_c(generic_reader_c *nreader,
                    int nsample_rate, int nchannels, int nbits_per_sample,
                    unsigned char *nheader, int nl_header,
                    track_info_t *nti) throw (error_c);
  virtual ~flac_packetizer_c();

  virtual int process(unsigned char *data, int size, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();
};

#endif  // HAVE_FLAC_STREAM_DECODER_H
#endif  // __P_FLAC_H
