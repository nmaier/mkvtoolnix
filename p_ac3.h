/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_ac3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_ac3.h,v 1.14 2003/05/05 21:55:02 mosu Exp $
    \brief class definition for the AC3 output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_AC3_H
#define __P_AC3_H

#include "common.h"
#include "pr_generic.h"
#include "ac3_common.h"

class ac3_packetizer_c: public generic_packetizer_c {
private:
  int64_t bytes_output, packetno;
  unsigned long samples_per_sec;
  int channels, buffer_size;
  unsigned char *packet_buffer;

public:
  ac3_packetizer_c(generic_reader_c *nreader, unsigned long nsamples_per_sec,
                   int nchannels, track_info_t *nti) throw (error_c);
  virtual ~ac3_packetizer_c();

  virtual int process(unsigned char *buf, int size, int64_t timecode = -1,
                      int64_t length = -1, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();
    
private:
  virtual void add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_ac3_packet(unsigned long *header,
                                        ac3_header_t *ac3header);
  virtual int ac3_packet_available();
  virtual void remove_ac3_packet(int pos, int framesize);
};

#endif // __P_AC3_H
