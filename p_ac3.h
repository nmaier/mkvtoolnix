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
    \version \$Id: p_ac3.h,v 1.6 2003/03/04 10:16:28 mosu Exp $
    \brief class definition for the AC3 output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_AC3_H
#define __P_AC3_H

#include "common.h"
#include "queue.h"
#include "ac3_common.h"

class ac3_packetizer_c: public q_c {
private:
  u_int64_t      bytes_output, packetno;
  unsigned long  samples_per_sec;
  int            channels;
  int            bitrate;
  audio_sync_t   async;
  unsigned char *packet_buffer;
  int            buffer_size;

public:
  ac3_packetizer_c(unsigned char *nprivate_data, int nprivate_size,
                   unsigned long nsamples_per_sec, int nchannels,
                   int nbitrate, audio_sync_t *nasync) throw (error_c);
  virtual ~ac3_packetizer_c();
    
  virtual int            process(unsigned char *buf, int size, int last_frame);
  virtual void           set_header();
    
  virtual void           set_params(unsigned long nsamples_per_sec,
                                    int nchannels, int nbitrate);
private:
  virtual void           add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_ac3_packet(unsigned long *header,
                                        ac3_header_t *ac3header);
  virtual int            ac3_packet_available();
  virtual void           remove_ac3_packet(int pos, int framesize);
};

#endif // __P_AC3_H
