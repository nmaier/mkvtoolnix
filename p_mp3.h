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
    \version \$Id: p_mp3.h,v 1.5 2003/03/04 09:27:05 mosu Exp $
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
  char          *tempbuf;
  audio_sync_t   async;
  char          *packet_buffer;
  int            buffer_size;

public:
  mp3_packetizer_c(void *pr_data, int pd_size,
                   unsigned long nsamples_per_sec, int nchannels,
                   int nmp3rate, audio_sync_t *nasync) throw (error_c);
  virtual ~mp3_packetizer_c();
    
  virtual int     process(char *buf, int size, int last_frame);
  virtual void    set_header();

private:
  virtual void    add_to_buffer(char *buf, int size);
  virtual char   *get_mp3_packet(unsigned long *header,
                                 mp3_header_t *mp3header);
  virtual int     mp3_packet_available();
  virtual void    remove_mp3_packet(int pos, int framesize);
};

#endif // __P_MP3_H
