/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_mp3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_mp3.h,v 1.17 2003/05/20 06:30:24 mosu Exp $
    \brief class definition for the MP3 output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_MP3_H
#define __P_MP3_H

#include "common.h"
#include "pr_generic.h"
#include "mp3_common.h"

class mp3_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno;
  unsigned long  samples_per_sec;
  int channels, buffer_size;
  unsigned char *packet_buffer;

public:
  mp3_packetizer_c(generic_reader_c *nreader, unsigned long nsamples_per_sec,
                   int nchannels, track_info_t *nti) throw (error_c);
  virtual ~mp3_packetizer_c();

  virtual int process(unsigned char *buf, int size, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

private:
  virtual void add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_mp3_packet(unsigned long *header,
                                        mp3_header_t *mp3header);
  virtual int mp3_packet_available();
  virtual void remove_mp3_packet(int pos, int framesize);
};

#endif // __P_MP3_H
