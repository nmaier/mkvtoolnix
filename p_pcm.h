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
    \version \$Id: p_pcm.h,v 1.7 2003/03/05 13:51:20 mosu Exp $
    \brief class definition for the PCM output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_PCM_H
#define __P_PCM_H

#include "common.h"
#include "queue.h"

class pcm_packetizer_c: public q_c {
private:
  int            packetno;
  int            bps;
  u_int64_t      bytes_output, remaining_sync;
  unsigned long  samples_per_sec;
  int            channels;
  int            bits_per_sample;
  unsigned char *tempbuf;
  
public:
  pcm_packetizer_c(unsigned long nsamples_per_sec, int nchannels,
                   int nbits_per_sample, track_info_t *nti) throw (error_c);
  virtual ~pcm_packetizer_c();
    
  virtual int  process(unsigned char *buf, int size, int last_frame);
  virtual void set_header();
};

const int pcm_interleave = 16;

#endif // __P_PCM_H
