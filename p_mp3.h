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
    \version \$Id: p_mp3.h,v 1.7 2003/03/05 13:51:20 mosu Exp $
    \brief class definition for the MP3 output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_MP3_H
#define __P_MP3_H

#include "common.h"
#include "queue.h"
#include "mp3_common.h"

class mp3_packetizer_c: public q_c {
private:
  u_int64_t      bytes_output, packetno;
  unsigned long  samples_per_sec;
  int            channels;
  int            mp3rate;
  unsigned char *tempbuf, *packet_buffer;
  int            buffer_size;

public:
  mp3_packetizer_c(unsigned long nsamples_per_sec, int nchannels,
                   int nmp3rate, track_info_t *nti) throw (error_c);
  virtual ~mp3_packetizer_c();
    
  virtual int            process(unsigned char *buf, int size, int last_frame);
  virtual void           set_header();

private:
  virtual void           add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_mp3_packet(unsigned long *header,
                                        mp3_header_t *mp3header);
  virtual int            mp3_packet_available();
  virtual void           remove_mp3_packet(int pos, int framesize);
};

#endif // __P_MP3_H
