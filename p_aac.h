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
    \version \$Id: p_aac.h,v 1.6 2003/05/22 11:11:05 mosu Exp $
    \brief class definition for the AAC output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_AAC_H
#define __P_AAC_H

#include "common.h"
#include "pr_generic.h"
#include "aac_common.h"

#define AAC_ID_MPEG4 0
#define AAC_ID_MPEG2 1

#define AAC_PROFILE_MAIN 0
#define AAC_PROFILE_LC   1
#define AAC_PROFILE_SSR  2
#define AAC_PROFILE_LTP  3

class aac_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno;
  unsigned long samples_per_sec;
  int channels, buffer_size, id, profile;
  unsigned char *packet_buffer;
  bool headerless;

public:
  aac_packetizer_c(generic_reader_c *nreader, int nid, int nprofile,
                   unsigned long nsamples_per_sec, int nchannels,
                   track_info_t *nti, bool nheaderless = false)
    throw (error_c);
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
