/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_aac.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_aac.h,v 1.5 2003/05/20 06:30:24 mosu Exp $
    \brief class definition for the AAC output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_AAC_H
#define __P_AAC_H

#include "common.h"
#include "pr_generic.h"
#include "aac_common.h"

class aac_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno;
  unsigned long samples_per_sec;
  int channels, buffer_size, id, profile;
  unsigned char *packet_buffer;

public:
  aac_packetizer_c(generic_reader_c *nreader, int nid, int nprofile,
                   unsigned long nsamples_per_sec, int nchannels,
                   track_info_t *nti) throw (error_c);
  virtual ~aac_packetizer_c();

  virtual int process(unsigned char *buf, int size, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

private:
  virtual void add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_aac_packet(unsigned long *header,
                                        aac_header_t *aacheader);
  virtual int aac_packet_available();
  virtual void remove_aac_packet(int pos, int framesize);
};

#endif // __P_AAC_H
